//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/passes.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"
#include "vpux/compiler/utils/types.hpp"

#include <ngraph/coordinate_diff.hpp>
#include <ngraph/op/op.hpp>

#include <mlir/IR/BlockAndValueMapping.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/DialectConversion.h>

using namespace vpux;

namespace {

//
// ConvertScalarToTensorPass
//

class ConvertScalarToTensorPass final : public IE::ConvertScalarToTensorBase<ConvertScalarToTensorPass> {
public:
    explicit ConvertScalarToTensorPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

public:
    class GatherScalarConverter;

private:
    void safeRunOnFunc() final;
};

//
// GenericConverter
//

mlir::LogicalResult convertGeneric(mlir::Operation* origOp, mlir::ValueRange operands,
                                   mlir::ConversionPatternRewriter& rewriter, mlir::TypeConverter& typeConverter,
                                   Logger log) {
    log.trace("Process Operation '{0}' at '{1}", origOp->getName(), origOp->getLoc());
    const auto origOperands = origOp->getOperands();
    VPUX_THROW_UNLESS(origOperands.size() == operands.size(), "Wrong operands size : {0}", operands.size());

    mlir::BlockAndValueMapping mapper;
    mapper.map(origOperands, operands);

    auto* newOp = rewriter.clone(*origOp, mapper);
    for (auto result : newOp->getResults()) {
        result.setType(typeConverter.convertType(result.getType()));
    }

    rewriter.replaceOp(origOp, newOp->getResults());
    return mlir::success();
}

class GenericConverter final : public mlir::OpInterfaceConversionPattern<IE::LayerOpInterface> {
public:
    GenericConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpInterfaceConversionPattern<IE::LayerOpInterface>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::LayerOpInterface origOp, ArrayRef<mlir::Value> newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final {
        auto* typeConverter = this->getTypeConverter();
        VPUX_THROW_UNLESS(typeConverter != nullptr, "TypeConverter was not set");

        return convertGeneric(origOp, newArgs, rewriter, *typeConverter, _log);
    }

private:
    Logger _log;
};

//
// GatherScalarConverter
//

class ConvertScalarToTensorPass::GatherScalarConverter final : public mlir::OpConversionPattern<IE::GatherOp> {
public:
    GatherScalarConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::GatherOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::GatherOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult ConvertScalarToTensorPass::GatherScalarConverter::matchAndRewrite(
        IE::GatherOp origOp, OpAdaptor newArgs, mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("Got '{0}' at '{1}'", origOp->getName(), origOp->getLoc());

    vpux::IE::GatherOp newOp;
    if ((origOp.axis() != nullptr) && (origOp.axis().getType().cast<vpux::NDTypeInterface>().getRank() == 0)) {
        const std::array<int64_t, 1> tensorShape = {1};
        const auto shapeAttr = getIntArrayAttr(getContext(), tensorShape);

        _log.nest().trace("New axis type '{0}'", newArgs.axis().getType());

        auto axisReshape = rewriter.create<IE::ReshapeOp>(origOp->getLoc(), newArgs.axis().getType(), origOp.axis(),
                                                          nullptr, true, shapeAttr);
        newOp = rewriter.create<IE::GatherOp>(origOp.getLoc(), origOp.input(), newArgs.indices(), axisReshape.output(),
                                              origOp.axis_valueAttr(), origOp.batch_dims());
    } else {
        newOp = rewriter.create<IE::GatherOp>(origOp.getLoc(), origOp.input(), newArgs.indices(), nullptr,
                                              origOp.axis_valueAttr(), origOp.batch_dims());
    }
    _log.nest().trace("New indices type '{0}'", newArgs.indices().getType());

    const auto origShapeAttr = getIntArrayAttr(origOp->getContext(), getShape(origOp.output()));
    auto reshapeOp = rewriter.create<IE::ReshapeOp>(origOp.getLoc(), origOp.getType(), newOp.output(), nullptr, nullptr,
                                                    origShapeAttr);
    rewriter.replaceOp(origOp, reshapeOp.output());

    return mlir::success();
}

//
// safeRunOnFunc
//

void ConvertScalarToTensorPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();

    const auto reshape = [](mlir::OpBuilder& builder, mlir::RankedTensorType dstType, mlir::ValueRange inputs,
                            mlir::Location loc) -> mlir::Value {
        VPUX_THROW_UNLESS(inputs.size() == 1, "Got wrong number of inputs : {0}", inputs.size());

        const auto outShapeAttr = builder.getI64ArrayAttr(dstType.getShape());
        return builder.createOrFold<IE::ReshapeOp>(loc, inputs.front(), nullptr, false, outShapeAttr);
    };

    mlir::TypeConverter typeConverter;
    typeConverter.addConversion([](vpux::NDTypeInterface type) {
        if (type.getRank() == 0) {
            const std::array<int64_t, 1> tensorShape = {1};
            const auto newTensorType = type.changeShape(ShapeRef(tensorShape));
            return newTensorType;
        }
        return type;
    });
    typeConverter.addSourceMaterialization(reshape);
    typeConverter.addTargetMaterialization(reshape);
    typeConverter.addArgumentMaterialization(reshape);

    mlir::ConversionTarget target(ctx);

    target.addLegalOp<IE::ReshapeOp, IE::AffineReshapeOp>();

    target.markUnknownOpDynamicallyLegal([&](mlir::Operation* op) {
        if (!mlir::isa<IE::LayerOpInterface>(op) || mlir::isa<IE::GatherOp>(op)) {
            return true;
        }
        for (auto operand : op->getOperands()) {
            if (operand.getType().cast<vpux::NDTypeInterface>().getRank() == 0) {
                return false;
            }
        }
        for (auto result : op->getResults()) {
            if (result.getType().cast<vpux::NDTypeInterface>().getRank() == 0) {
                return false;
            }
        }
        return true;
    });

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<GenericConverter>(typeConverter, &ctx, _log);

    if (mlir::failed(mlir::applyPartialConversion(func, target, std::move(patterns)))) {
        _log.debug("Failed to replace operand from scalar to tensor");
        signalPassFailure();
    }

    mlir::ConversionTarget targetGather(ctx);

    targetGather.addLegalOp<IE::ReshapeOp, IE::AffineReshapeOp>();

    targetGather.addDynamicallyLegalOp<IE::GatherOp>([&](IE::GatherOp op) {
        return typeConverter.isLegal(op);
    });

    mlir::RewritePatternSet patternsGather(&ctx);
    patternsGather.add<GatherScalarConverter>(typeConverter, &ctx, _log);

    if (mlir::failed(mlir::applyPartialConversion(func, targetGather, std::move(patternsGather)))) {
        _log.debug("Failed to replace indices from scalar to tensor");
        signalPassFailure();
    }
}

}  // namespace

//
// createConvertScalarToTensorPass
//

std::unique_ptr<mlir::Pass> vpux::IE::createConvertScalarToTensorPass(Logger log) {
    return std::make_unique<ConvertScalarToTensorPass>(log);
}

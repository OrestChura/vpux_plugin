//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"

#include "vpux/utils/core/checked_cast.hpp"

using namespace vpux;

mlir::LogicalResult vpux::IE::LogSoftmaxOp::inferReturnTypeComponents(
        mlir::MLIRContext* ctx, Optional<mlir::Location> optLoc, mlir::ValueShapeRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange,
        SmallVectorImpl<mlir::ShapedTypeComponents>& inferredReturnShapes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    IE::LogSoftmaxOpAdaptor logSoftmax(operands, attrs);
    if (mlir::failed(logSoftmax.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = logSoftmax.input().getType().cast<mlir::ShapedType>();
    inferredReturnShapes.emplace_back(inType.getShape(), inType.getElementType());

    return mlir::success();
}

mlir::OpFoldResult vpux::IE::LogSoftmaxOp::fold(ArrayRef<mlir::Attribute>) {
    // This folder is to simplify a special case when there is only one element along the axis, i.e. inShape[axis] == 1
    // In this case, all elements in the output should be log(exp(0)/exp(0)) i.e. 0.0f.
    const auto inType = input().getType().cast<mlir::ShapedType>();
    const auto inShape = inType.getShape();
    const auto inRank = inType.getRank();

    auto axis = checked_cast<int64_t>(axisInd());

    if (axis < 0) {
        axis += inRank;
    }

    VPUX_THROW_UNLESS(axis >= 0 && axis < inRank, "Wrong LogSoftmax axis {0}", axis);

    if (inShape[axis] > 1) {
        return nullptr;
    }

    const auto valueType = mlir::RankedTensorType::get(inShape, mlir::Float32Type::get(getContext()));
    const auto baseContent = Const::ContentAttr::get(mlir::DenseElementsAttr::get(valueType, 0.0f));

    return baseContent.convertElemType(output().getType().cast<mlir::ShapedType>().getElementType());
}

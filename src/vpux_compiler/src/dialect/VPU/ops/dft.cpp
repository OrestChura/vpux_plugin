//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

//

#include "vpux/compiler/dialect/VPU/ops.hpp"

#include "vpux/compiler/core/tiling.hpp"
#include "vpux/compiler/dialect/IE/utils/shape_infer.hpp"
#include "vpux/compiler/utils/attributes.hpp"

using namespace vpux;

namespace {

struct DFTInputData final {
    SmallVector<int64_t> axes;
    SmallVector<int64_t> signal_size;
};

mlir::FailureOr<DFTInputData> extractData(VPU::DFTOpAdaptor op) {
    if (op.axes_attr() && op.signal_size_attr()) {
        auto axes = parseIntArrayAttr<int64_t>(op.axes_attr());
        auto signal_size = parseIntArrayAttr<int64_t>(op.signal_size_attr());
        return DFTInputData{std::move(axes), std::move(signal_size)};
    }
    return mlir::failure();
}

}  // namespace

mlir::LogicalResult vpux::VPU::DFTOp::inferReturnTypes(mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc,
                                                       mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                       mlir::RegionRange /*regions*/,

                                                       mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.getValueOr(mlir::UnknownLoc::get(ctx));
    VPU::DFTOpAdaptor op(operands, attrs);
    if (mlir::failed(op.verify(loc))) {
        return mlir::failure();
    }
    const auto inputData = extractData(op);
    SmallVector<int64_t> axes = inputData.getValue().axes;
    SmallVector<int64_t> signal_size = inputData.getValue().signal_size;

    const auto inType = op.input().getType().cast<vpux::NDTypeInterface>();
    auto outShape = to_small_vector(inType.getShape());

    for (size_t i = 0; i < axes.size(); ++i) {
        const int64_t current_axis = axes[i];
        if (signal_size[i] != -1) {
            outShape[current_axis] = signal_size[i];
        }
    }

    auto outType = inType.changeShape(Shape(outShape));
    inferredReturnTypes.push_back(outType);
    return mlir::success();
}

void vpux::VPU::DFTOp::inferLayoutInfo(mlir::Operation*, IE::LayerLayoutInfo& info) {
    IE::fillDefaultLayoutInfo(info);
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::DFTOp::serialize(EMU::BlobWriter& /*writer*/) {
    VPUX_THROW("DFT is not implemented in UPA Tasks.");
}

//
// TilingBuilderOpInterface
//

vpux::InputTiling vpux::VPU::DFTOp::backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger /*log*/) {
    auto curTile = outputTile;
    SmallVector<int64_t> axes = parseIntArrayAttr<int64_t>(axes_attr());
    const auto inShape = getShape(input());
    for (auto axis : axes) {
        curTile.shape[Dim(axis)] = inShape[Dim(axis)];
    }
    return TilingInfo{curTile};
}

void vpux::VPU::DFTOp::adjustAttrs(const TilingInfo& /*inputTiling*/, const TileInfo& /*outputTile*/) {
}

OutputTiling vpux::VPU::DFTOp::getTilingStrategy(TilingMode tilingMode, Logger log) {
    auto op = getOperation();
    // eliminate axes from possible tiling dims
    SmallVector<int64_t> axes = parseIntArrayAttr<int64_t>(axes_attr());
    // add last axis to not allowed split as represent the complex number
    const auto outputType = op->getResult(0).getType().cast<vpux::NDTypeInterface>();
    const auto outputShape = outputType.getShape();
    axes.push_back(outputShape.size() - 1);
    return getSWLayerTilingStrategyWithAxesExclusion(op, tilingMode, log, axes);
}
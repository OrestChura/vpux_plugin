//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"

using namespace vpux;

mlir::LogicalResult vpux::VPU::DynamicQuantizeOp::inferReturnTypes(
        mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc, mlir::ValueRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange /*regions*/,
        mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::DynamicQuantizeOpAdaptor quantize(operands, attrs);
    if (mlir::failed(quantize.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = quantize.input().getType().cast<vpux::NDTypeInterface>();

    SmallVector<int64_t> scalarShape{1};
    const auto outScalarType = inType.changeShape(Shape(scalarShape));
    auto ui8Type = mlir::IntegerType::get(ctx, 8, mlir::IntegerType::SignednessSemantics::Unsigned);

    inferredReturnTypes.emplace_back(inType.changeElemType(ui8Type));
    inferredReturnTypes.emplace_back(outScalarType.changeElemType(inType.getElementType()));
    inferredReturnTypes.emplace_back(outScalarType.changeElemType(ui8Type));
    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::DynamicQuantizeOp::serialize(EMU::BlobWriter&) {
    VPUX_THROW("VPU::DynamicQuantizeOp not supported");
}

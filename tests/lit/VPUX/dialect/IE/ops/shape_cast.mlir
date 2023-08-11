//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --canonicalize %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

// CHECK-LABEL: @Eliminate
func.func @Eliminate(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x2x3x4xf16> {
    %0 = IE.ShapeCast {shape = [1, 2, 3, 4]} inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x2x3x4xf16>
    return %0 : tensor<1x2x3x4xf16>

    // CHECK-NOT:   IE.ShapeCast
    // CHECK:       return %arg0
}

// -----

// CHECK-LABEL: @Fuse
func.func @Fuse(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x2x3x4xf16> {
    %0 = IE.ShapeCast {shape = [1, 3, 2, 4]} inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x3x2x4xf16>
    %1 = IE.ShapeCast {shape = [1, 2, 3, 4]} inputs(%0 : tensor<1x3x2x4xf16>) -> tensor<1x2x3x4xf16>
    return %1 : tensor<1x2x3x4xf16>

    // CHECK-NOT:   IE.ShapeCast
    // CHECK:       return %arg0
}

// -----

// CHECK-LABEL: @FuseSequence
func.func @FuseSequence(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x4x3x2xf16> {
    %0 = IE.ShapeCast {shape = [1, 3, 2, 4]} inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x3x2x4xf16>
    %1 = IE.ShapeCast {shape = [1, 3, 4, 2]} inputs(%0 : tensor<1x3x2x4xf16>) -> tensor<1x3x4x2xf16>
    %2 = IE.ShapeCast {shape = [1, 4, 3, 2]} inputs(%1 : tensor<1x3x4x2xf16>) -> tensor<1x4x3x2xf16>
    return %2 : tensor<1x4x3x2xf16>

    // CHECK:       [[SHAPE_CAST:%.+]] = IE.ShapeCast {shape = [1, 4, 3, 2]} inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x4x3x2xf16>
    // CHECK:       return [[SHAPE_CAST]]
}

// -----

#NCWH = affine_map<(d0, d1, d2, d3) -> (d0, d1, d3, d2)>
#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>

// CHECK-LABEL: @FuseShapeCastWithMultipleBranches
func.func @FuseShapeCastWithMultipleBranches(%arg0 : tensor<1x2x3x4xf16>) ->
    (tensor<1x3x2x4xf16>, tensor<1x3x4x2xf16, { order = #NCWH }>)
{
    %0 = IE.ShapeCast { shape = [1, 3, 2, 4] } inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x3x2x4xf16>

    %1 = IE.ShapeCast { shape = [1, 3, 2, 4] } inputs(%0 : tensor<1x3x2x4xf16>) -> tensor<1x3x2x4xf16>
    %2 = IE.PermuteCast(%0) { dst_order = #NCWH, mem_perm = #NCHW } : tensor<1x3x2x4xf16> -> tensor<1x3x4x2xf16, { order = #NCWH }>

    return %0, %2 : tensor<1x3x2x4xf16>, tensor<1x3x4x2xf16, { order = #NCWH }>

    // CHECK: [[SHAPECAST:%.+]] = IE.ShapeCast {shape = [1, 3, 2, 4]} inputs(%arg0 : tensor<1x2x3x4xf16>) -> tensor<1x3x2x4xf16>
    // CHECK: [[PERMUTECAST:%.+]] = IE.PermuteCast([[SHAPECAST]]) {dst_order = #NCWH, mem_perm = #NCHW} : tensor<1x3x2x4xf16> -> tensor<1x3x4x2xf16, {order = #NCWH}>
    // CHECK: return [[SHAPECAST]], [[PERMUTECAST]] : tensor<1x3x2x4xf16>, tensor<1x3x4x2xf16, {order = #NCWH}>
}

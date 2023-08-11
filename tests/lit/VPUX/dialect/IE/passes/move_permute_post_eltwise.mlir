//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --move-permute-post-eltwise --canonicalize %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>
!qElemType0 = !quant.uniform<u8:f16, 0.001>
!qElemType1 = !quant.uniform<u8:f16, 0.002>
!qElemType2 = !quant.uniform<u8:f16, 0.003>
!qElemType3 = !quant.uniform<u8:f16, 0.004>
!qElemType4 = !quant.uniform<u8:f16, 0.005>


// CHECK-LABEL: @MovePermutePostAdd
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x256x256xf16>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x256x256xf16>
func.func @MovePermutePostAdd(%arg0: tensor<1x3x256x256xf16>, %arg1: tensor<1x3x256x256xf16>) -> tensor<1x3x256x256xf16, {order = #NHWC}> {
    %0 = IE.MemPermute(%arg0) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    %1 = IE.MemPermute(%arg1) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256xf16, {order = #NHWC}>, tensor<1x3x256x256xf16, {order = #NHWC}>
            -> tensor<1x3x256x256xf16, {order = #NHWC}>
    return %2 : tensor<1x3x256x256xf16, {order = #NHWC}>

    // CHECK:       [[PERMUTE_CAST_1:%.+]] = IE.PermuteCast([[INPUT1]]) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x3x256x256xf16> -> tensor<1x256x3x256xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_1:%.+]] = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[PERMUTE_CAST_1]] : tensor<1x256x3x256xf16, {order = #NHWC}>) -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[PERMUTE_CAST_2:%.+]] = IE.PermuteCast([[INPUT2]]) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x3x256x256xf16> -> tensor<1x256x3x256xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_2:%.+]] = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[PERMUTE_CAST_2]] : tensor<1x256x3x256xf16, {order = #NHWC}>) -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST_1]], [[SHAPE_CAST_2]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256xf16, {order = #NHWC}>, tensor<1x3x256x256xf16, {order = #NHWC}> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 256, 3, 256]} inputs([[ADD]] : tensor<1x3x256x256xf16, {order = #NHWC}>) -> tensor<1x256x3x256xf16, {order = #NHWC}>
    // CHECK:       [[MEMPERMUTE:%.+]] = IE.MemPermute([[SHAPE_CAST_OUT]]) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x256x3x256xf16, {order = #NHWC}> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       return [[MEMPERMUTE]] : tensor<1x3x256x256xf16, {order = #NHWC}>
}

// CHECK-LABEL: @MovePermutePostAddWithOneInput
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x256x256xf16>
func.func @MovePermutePostAddWithOneInput(%arg0: tensor<1x3x256x256xf16>) -> tensor<1x3x256x256xf16, {order = #NHWC}> {
    %0 = IE.MemPermute(%arg0) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    %2 = IE.Add(%0, %0) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256xf16, {order = #NHWC}>, tensor<1x3x256x256xf16, {order = #NHWC}>
            -> tensor<1x3x256x256xf16, {order = #NHWC}>
    return %2 : tensor<1x3x256x256xf16, {order = #NHWC}>

    // CHECK:       [[PERMUTE_CAST:%.+]] = IE.PermuteCast([[INPUT]]) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x3x256x256xf16> -> tensor<1x256x3x256xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST:%.+]] = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[PERMUTE_CAST]] : tensor<1x256x3x256xf16, {order = #NHWC}>) -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST]], [[SHAPE_CAST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256xf16, {order = #NHWC}>, tensor<1x3x256x256xf16, {order = #NHWC}> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 256, 3, 256]} inputs([[ADD]] : tensor<1x3x256x256xf16, {order = #NHWC}>) -> tensor<1x256x3x256xf16, {order = #NHWC}>
    // CHECK:       [[MEMPERMUTE:%.+]] = IE.MemPermute([[SHAPE_CAST_OUT]]) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x256x3x256xf16, {order = #NHWC}> -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       return [[MEMPERMUTE]] : tensor<1x3x256x256xf16, {order = #NHWC}>
}

// CHECK-LABEL: @MovePermutePostAddWithQuantizeCast
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x256x256x!qElemType0>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x256x256x!qElemType1>
func.func @MovePermutePostAddWithQuantizeCast(%arg0: tensor<1x3x256x256x!qElemType0>, %arg1: tensor<1x3x256x256x!qElemType1>)
        -> tensor<1x3x256x256x!qElemType0, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}> {
    %permute1 = IE.MemPermute(%arg0) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256x!qElemType0> -> tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
    %qc1 = IE.QuantizeCast(%permute1) {dstElemType = !qElemType2} : tensor<1x3x256x256x!qElemType0, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
            -> tensor<1x3x256x256x!qElemType2, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
    %permute2 = IE.MemPermute(%arg1) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256x!qElemType1> -> tensor<1x3x256x256x!qElemType1, {order = #NHWC}>
    %qc2 = IE.QuantizeCast(%permute2) {dstElemType = !qElemType3} : tensor<1x3x256x256x!qElemType1, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
            -> tensor<1x3x256x256x!qElemType3, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
    %add = IE.Add(%qc1, %qc2) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256x!qElemType2, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>, tensor<1x3x256x256x!qElemType3, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
            -> tensor<1x3x256x256x!qElemType4, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
    %qc3 = IE.QuantizeCast(%add) {dstElemType = !qElemType0} : tensor<1x3x256x256x!qElemType4, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
            -> tensor<1x3x256x256x!qElemType0, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>
    return %qc3 : tensor<1x3x256x256x!qElemType0, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}>

    // CHECK:       [[PERM_CAST_1:%.+]] = IE.PermuteCast([[INPUT1]]) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x3x256x256x!qElemType0>
    // CHECK-SAME:          -> tensor<1x256x3x256x!qElemType0, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_1:%.+]] = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[PERM_CAST_1]] : tensor<1x256x3x256x!qElemType0, {order = #NHWC}>)
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
    // CHECK:       [[QUANT_CAST_1:%.+]] = IE.QuantizeCast([[SHAPE_CAST_1]]) {dstElemType = !qElemType2} : tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType2, {order = #NHWC}>
    // CHECK:       [[PERM_CAST_2:%.+]] = IE.PermuteCast([[INPUT2]]) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x3x256x256x!qElemType1>
    // CHECK-SAME:          -> tensor<1x256x3x256x!qElemType1, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_2:%.+]] = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[PERM_CAST_2]] : tensor<1x256x3x256x!qElemType1, {order = #NHWC}>)
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType1, {order = #NHWC}>
    // CHECK:       [[QUANT_CAST_2:%.+]] = IE.QuantizeCast([[SHAPE_CAST_2]]) {dstElemType = !qElemType3} : tensor<1x3x256x256x!qElemType1, {order = #NHWC}>
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType3, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[QUANT_CAST_1]], [[QUANT_CAST_2]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x256x256x!qElemType2, {order = #NHWC}>, tensor<1x3x256x256x!qElemType3, {order = #NHWC}>
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType4, {order = #NHWC}>
    // CHECK:       [[QUANT_CAST_OUT:%.+]] = IE.QuantizeCast([[ADD]]) {dstElemType = !qElemType0} : tensor<1x3x256x256x!qElemType4, {order = #NHWC}>
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 256, 3, 256]} inputs([[QUANT_CAST_OUT]] : tensor<1x3x256x256x!qElemType0, {order = #NHWC}>)
    // CHECK-SAME:          -> tensor<1x256x3x256x!qElemType0, {order = #NHWC}>
    // CHECK:       [[MEMPERMUTE:%.+]] = IE.MemPermute([[SHAPE_CAST_OUT]]) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x256x3x256x!qElemType0, {order = #NHWC}>
    // CHECK-SAME:          -> tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
    // CHECK:       return [[MEMPERMUTE]] : tensor<1x3x256x256x!qElemType0, {order = #NHWC}>
}

// CHECK-LABEL: @MovePermutePostAddWithSameLayout
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x256x256xf16, {order = #NHWC}>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x256x256xf16, {order = #NHWC}>
func.func @MovePermutePostAddWithSameLayout(%arg0: tensor<1x3x256x256xf16, {order = #NHWC}>, %arg1: tensor<1x3x256x256xf16, {order = #NHWC}>) -> tensor<1x256x256x3xf16, {order = #NHWC}> {
    %0 = IE.MemPermute(%arg0) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16, {order = #NHWC}> -> tensor<1x256x256x3xf16, {order = #NHWC}>
    %1 = IE.MemPermute(%arg1) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16, {order = #NHWC}> -> tensor<1x256x256x3xf16, {order = #NHWC}>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x256x256x3xf16, {order = #NHWC}>, tensor<1x256x256x3xf16, {order = #NHWC}>
            -> tensor<1x256x256x3xf16, {order = #NHWC}>
    return %2 : tensor<1x256x256x3xf16, {order = #NHWC}>


    // CHECK:       [[SHAPE_CAST_1:%.+]]  = IE.ShapeCast {shape = [1, 256, 256, 3]} inputs([[INPUT1]] : tensor<1x3x256x256xf16, {order = #NHWC}>) -> tensor<1x256x256x3xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_2:%.+]]  = IE.ShapeCast {shape = [1, 256, 256, 3]} inputs([[INPUT2]] : tensor<1x3x256x256xf16, {order = #NHWC}>) -> tensor<1x256x256x3xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]]  = IE.Add([[SHAPE_CAST_1]], [[SHAPE_CAST_2]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x256x256x3xf16, {order = #NHWC}>, tensor<1x256x256x3xf16, {order = #NHWC}> -> tensor<1x256x256x3xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]]  = IE.ShapeCast {shape = [1, 3, 256, 256]} inputs([[ADD]] : tensor<1x256x256x3xf16, {order = #NHWC}>) -> tensor<1x3x256x256xf16, {order = #NHWC}>
    // CHECK:       [[MEMPERMUTE:%.+]]  = IE.MemPermute([[SHAPE_CAST_OUT]]) {dst_order = #NHWC, mem_perm = #NHWC} : tensor<1x3x256x256xf16, {order = #NHWC}> -> tensor<1x256x256x3xf16, {order = #NHWC}>
    // CHECK:       return [[MEMPERMUTE]] : tensor<1x256x256x3xf16, {order = #NHWC}>
}

// CHECK-LABEL: @MovePermutePostAddDifferentDstOrderAndMemPerm
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x512x64x8xf16>
func.func @MovePermutePostAddDifferentDstOrderAndMemPerm(%arg0: tensor<1x512x64x8xf16>) -> tensor<1x64x8x512x!qElemType0, {order = #NHWC}> {
    %0 = IE.MemPermute(%arg0) {dst_order = #NHWC, mem_perm = #NWCH} : tensor<1x512x64x8xf16> -> tensor<1x64x8x512xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x64x8x512xf16, {order = #NHWC}>, tensor<1x64x8x512xf16, {order = #NHWC}> -> tensor<1x64x8x512x!qElemType1, {order = #NHWC}>
    %2 = IE.QuantizeCast(%1) {dstElemType = !qElemType0} : tensor<1x64x8x512x!qElemType1, {order = #NHWC}> -> tensor<1x64x8x512x!qElemType0, {order = #NHWC}>
    return %2 : tensor<1x64x8x512x!qElemType0, {order = #NHWC}>

    // CHECK:       [[PERM_CAST_1:%.+]] = IE.PermuteCast(%arg0) {dst_order = #NHWC, mem_perm = #NCHW} : tensor<1x512x64x8xf16> -> tensor<1x8x512x64xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_1:%.+]] = IE.ShapeCast {shape = [1, 64, 8, 512]} inputs(%0 : tensor<1x8x512x64xf16, {order = #NHWC}>) -> tensor<1x64x8x512xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST_1]], [[SHAPE_CAST_1]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x64x8x512xf16, {order = #NHWC}>, tensor<1x64x8x512xf16, {order = #NHWC}> -> tensor<1x64x8x512x!qElemType1, {order = #NHWC}>
    // CHECK:       [[QUANT_CAST_OUT:%.+]] = IE.QuantizeCast([[ADD]]) {dstElemType = !qElemType0} : tensor<1x64x8x512x!qElemType1, {order = #NHWC}> -> tensor<1x64x8x512x!qElemType0, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_2:%.+]] = IE.ShapeCast {shape = [1, 8, 512, 64]} inputs(%3 : tensor<1x64x8x512x!qElemType0, {order = #NHWC}>) -> tensor<1x8x512x64x!qElemType0, {order = #NHWC}>
    // CHECK:       [[MEM_PERM:%.+]] = IE.MemPermute([[SHAPE_CAST_2]]) {dst_order = #NHWC, mem_perm = #NWCH} : tensor<1x8x512x64x!qElemType0, {order = #NHWC}> -> tensor<1x64x8x512x!qElemType0, {order = #NHWC}>
    // CHECK:       return [[MEM_PERM]] : tensor<1x64x8x512x!qElemType0, {order = #NHWC}>
}

// CHECK-LABEL: @DoNotMovePermutePostAddWhenDifferentMemPermAttributes
// CHECK-SAME:        [[INPUT1:%arg0]]: tensor<1x2x3x4xf16>
// CHECK-SAME:        [[INPUT1:%arg1]]: tensor<1x3x4x2xf16>
func.func @DoNotMovePermutePostAddWhenDifferentMemPermAttributes(%arg0: tensor<1x2x3x4xf16>, %arg1: tensor<1x3x4x2xf16>) -> tensor<1x4x2x3xf16> {
    %0 = IE.MemPermute(%arg0) {dst_order = #NCHW, mem_perm = #NWCH} : tensor<1x2x3x4xf16> -> tensor<1x4x2x3xf16>
    %1 = IE.MemPermute(%arg1) {dst_order = #NCHW, mem_perm = #NHWC} : tensor<1x3x4x2xf16> -> tensor<1x4x2x3xf16>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x4x2x3xf16>, tensor<1x4x2x3xf16> -> tensor<1x4x2x3xf16>
    return %2 : tensor<1x4x2x3xf16>

    // CHECK:       [[MEM_PERM_1:%.+]]  = IE.MemPermute(%arg0) {dst_order = #NCHW, mem_perm = #NWCH} : tensor<1x2x3x4xf16> -> tensor<1x4x2x3xf16>
    // CHECK:       [[MEM_PERM_2:%.+]]  = IE.MemPermute(%arg1) {dst_order = #NCHW, mem_perm = #NHWC} : tensor<1x3x4x2xf16> -> tensor<1x4x2x3xf16>
    // CHECK:       [[ADD:%.+]]  = IE.Add([[MEM_PERM_1]], [[MEM_PERM_2]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x4x2x3xf16>, tensor<1x4x2x3xf16> -> tensor<1x4x2x3xf16>
    // CHECK:       return [[ADD]] : tensor<1x4x2x3xf16>
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>
#NCWH = affine_map<(d0, d1, d2, d3) -> (d0, d1, d3, d2)>

// CHECK-LABEL: @PropagatePermuteThroughGroupConv
// CHECK-SAME:        [[BLOCK_ARG:%arg0]]: tensor<1x4x8x64xf16>
func.func @PropagatePermuteThroughGroupConv(%arg0: tensor<1x4x8x64xf16>) -> tensor<1x8x4x64xf16> {
    %SCALE = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01> : tensor<1x1x1x1xf16>, [
        #const.Broadcast<0 : i64, 16 : i64>,
        #const.Reshape<[16, 1, 1, 1]>,
        #const.Reorder<#NHWC>
    ]

    %IN_MEM_PERM = IE.MemPermute(%arg0) {
        dst_order = #NHWC,
        mem_perm = #NCWH
    } : tensor<1x4x8x64xf16> -> tensor<1x8x4x64xf16, {order = #NHWC}>

    %IN_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 16, 16, 8]
    } inputs(%IN_MEM_PERM : tensor<1x8x4x64xf16, {order = #NHWC}>) -> tensor<1x16x16x8xf16, {order = #NHWC}>

    %SCALE_SHIFT = IE.GroupConvolution(%IN_SHAPE_CAST, %SCALE) {
        dilations = [1, 1],
        groups = 16 : i64,
        pads_begin = [0, 0],
        pads_end = [0, 0],
        strides = [1, 1]
    } : tensor<1x16x16x8xf16, {order = #NHWC}>,
        tensor<16x1x1x1xf16, {order = #NHWC}>
            -> tensor<1x16x16x8xf16, {order = #NHWC}>

    %OUT_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 8, 4, 64]
    } inputs(%SCALE_SHIFT : tensor<1x16x16x8xf16, {order = #NHWC}>) -> tensor<1x8x4x64xf16, {order = #NHWC}>

    %OUT_MEM_PERM = IE.MemPermute(%OUT_SHAPE_CAST) {
        dst_order = #NCHW,
        mem_perm = #NWCH
    } : tensor<1x8x4x64xf16, {order = #NHWC}> -> tensor<1x8x4x64xf16>

    return %OUT_MEM_PERM : tensor<1x8x4x64xf16>

    // CHECK-DAG:       [[SCALE:%.*]] = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01>
    // CHECK:       [[PERMUTE_CAST:%.+]] = IE.PermuteCast([[BLOCK_ARG]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NCHW
    // CHECK-SAME:  } : tensor<1x4x8x64xf16> -> tensor<1x64x4x8xf16, {order = #NHWC}>

    // CHECK:       [[IN_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 16, 16, 8]
    // CHECK-SAME:  } inputs([[PERMUTE_CAST]] : tensor<1x64x4x8xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x16x16x8xf16, {order = #NHWC}>

    // CHECK:       [[GROUP_CONVOLUTION:%.+]] = IE.GroupConvolution([[IN_SHAPE_CAST]], [[SCALE]])

    // CHECK:       [[OUT_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 64, 4, 8]
    // CHECK-SAME:  } inputs([[GROUP_CONVOLUTION]] : tensor<1x16x16x8xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x64x4x8xf16, {order = #NHWC}>

    // CHECK:       [[OUT_MEM_PERMUTE:%.+]] = IE.MemPermute([[OUT_SHAPE_CAST]]) {
    // CHECK-SAME:      dst_order = #NCHW,
    // CHECK-SAME:      mem_perm = #NHCW
    // CHECK-SAME:  } : tensor<1x64x4x8xf16, {order = #NHWC}> -> tensor<1x8x4x64xf16>

    // CHECK:       return [[OUT_MEM_PERMUTE]] : tensor<1x8x4x64xf16>
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NCWH = affine_map<(d0, d1, d2, d3) -> (d0, d1, d3, d2)>
#NWHC = affine_map<(d0, d1, d2, d3) -> (d0, d3, d2, d1)>

// CHECK-LABEL: @PropagatePermuteThroughGroupConvCase2
// CHECK-SAME:        [[BLOCK_ARG:%arg0]]: tensor<1x4x8x64xf16, {order = #NCWH}>
func.func @PropagatePermuteThroughGroupConvCase2(%arg0: tensor<1x4x8x64xf16, {order = #NCWH}>)-> tensor<1x16x16x8xf16, {order = #NHWC}> {
    %SCALE = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01> : tensor<1x1x1x1xf16>, [
        #const.Broadcast<0 : i64, 16 : i64>,
        #const.Reshape<[16, 1, 1, 1]>,
        #const.Reorder<#NHWC>
    ]

    %IN_MEM_PERM = IE.MemPermute(%arg0) {
        dst_order = #NHWC,
        mem_perm = #NWHC
    } : tensor<1x4x8x64xf16,{order = #NCWH}> -> tensor<1x4x8x64xf16, {order = #NHWC}>

    %IN_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 16, 16, 8]
    } inputs(%IN_MEM_PERM : tensor<1x4x8x64xf16, {order = #NHWC}>) -> tensor<1x16x16x8xf16, {order = #NHWC}>

    %SCALE_SHIFT = IE.GroupConvolution(%IN_SHAPE_CAST, %SCALE) {
        dilations = [1, 1],
        groups = 16 : i64,
        pads_begin = [0, 0],
        pads_end = [0, 0],
        strides = [1, 1]
    } : tensor<1x16x16x8xf16, {order = #NHWC}>,
        tensor<16x1x1x1xf16, {order = #NHWC}>
            -> tensor<1x16x16x8xf16, {order = #NHWC}>

    return %SCALE_SHIFT : tensor<1x16x16x8xf16, {order = #NHWC}>

    // CHECK-DAG:       [[SCALE:%.*]] = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01>
    // CHECK:       [[PERMUTE_CAST:%.+]] = IE.PermuteCast([[BLOCK_ARG]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NCHW
    // CHECK-SAME:  } : tensor<1x4x8x64xf16, {order = #NCWH}> -> tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK:       [[IN_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 16, 16, 8]
    // CHECK-SAME:  } inputs([[PERMUTE_CAST]] : tensor<1x8x4x64xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x16x16x8xf16, {order = #NHWC}>

    // CHECK:       [[GROUP_CONVOLUTION:%.+]] = IE.GroupConvolution([[IN_SHAPE_CAST]], [[SCALE]])

    // CHECK:       [[OUT_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 8, 4, 64]
    // CHECK-SAME:  } inputs([[GROUP_CONVOLUTION]] : tensor<1x16x16x8xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK:       [[OUT_MEM_PERMUTE:%.+]] = IE.MemPermute([[OUT_SHAPE_CAST]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NWHC
    // CHECK-SAME:  } : tensor<1x8x4x64xf16, {order = #NHWC}> -> tensor<1x4x8x64xf16, {order = #NHWC}>

    // CHECK:       [[OUTPUT_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 16, 16, 8]
    // CHECK-SAME:  } inputs([[OUT_MEM_PERMUTE]] : tensor<1x4x8x64xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x16x16x8xf16, {order = #NHWC}>


    // CHECK:       return [[OUTPUT_SHAPE_CAST]] : tensor<1x16x16x8xf16, {order = #NHWC}>
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>
#NCWH = affine_map<(d0, d1, d2, d3) -> (d0, d1, d3, d2)>

// CHECK-LABEL: @SkipGroupConvWith2ShapeCastConsumers
// CHECK-SAME:        [[BLOCK_ARG:%arg0]]: tensor<1x4x8x64xf16>
func.func @SkipGroupConvWith2ShapeCastConsumers(%arg0: tensor<1x4x8x64xf16>)
        -> (tensor<1x8x4x64xf16, {order = #NHWC}>, tensor<1x8x4x64xf16, {order = #NHWC}>) {
    %SCALE = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01> : tensor<1x1x1x1xf16>, [
        #const.Broadcast<0 : i64, 16 : i64>,
        #const.Reshape<[16, 1, 1, 1]>,
        #const.Reorder<#NHWC>
    ]

    %IN_MEM_PERM = IE.MemPermute(%arg0) {
        dst_order = #NHWC,
        mem_perm = #NCWH
    } : tensor<1x4x8x64xf16> -> tensor<1x8x4x64xf16, {order = #NHWC}>

    %IN_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 16, 16, 8]
    } inputs(%IN_MEM_PERM : tensor<1x8x4x64xf16, {order = #NHWC}>) -> tensor<1x16x16x8xf16, {order = #NHWC}>

    %SCALE_SHIFT = IE.GroupConvolution(%IN_SHAPE_CAST, %SCALE) {
        dilations = [1, 1],
        groups = 16 : i64,
        pads_begin = [0, 0],
        pads_end = [0, 0],
        strides = [1, 1]
    } : tensor<1x16x16x8xf16, {order = #NHWC}>,
        tensor<16x1x1x1xf16, {order = #NHWC}>
            -> tensor<1x16x16x8xf16, {order = #NHWC}>

    %OUT_SHAPE_CAST_1 = IE.ShapeCast {
        shape = [1, 8, 4, 64]
    } inputs(%SCALE_SHIFT : tensor<1x16x16x8xf16, {order = #NHWC}>) -> tensor<1x8x4x64xf16, {order = #NHWC}>

    %OUT_SHAPE_CAST_2 = IE.ShapeCast {
        shape = [1, 8, 4, 64]
    } inputs(%SCALE_SHIFT : tensor<1x16x16x8xf16, {order = #NHWC}>) -> tensor<1x8x4x64xf16, {order = #NHWC}>

    return %OUT_SHAPE_CAST_1, %OUT_SHAPE_CAST_2 : tensor<1x8x4x64xf16, {order = #NHWC}>, tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK-DAG:       [[SCALE:%.*]] = const.Declare tensor<16x1x1x1xf16, {order = #NHWC}> = dense<1.250000e-01>
    // CHECK:       [[IN_MEM_PERM:%.+]] = IE.MemPermute([[BLOCK_ARG]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NCWH
    // CHECK-SAME:  } : tensor<1x4x8x64xf16> -> tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK:       [[IN_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 16, 16, 8]
    // CHECK-SAME:  } inputs([[IN_MEM_PERM]] : tensor<1x8x4x64xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x16x16x8xf16, {order = #NHWC}>

    // CHECK:       [[GROUP_CONVOLUTION:%.+]] = IE.GroupConvolution([[IN_SHAPE_CAST]], [[SCALE]])

    // CHECK:       [[OUT_SHAPE_CAST_1:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 8, 4, 64]
    // CHECK-SAME:  } inputs([[GROUP_CONVOLUTION]] : tensor<1x16x16x8xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK:       [[OUT_SHAPE_CAST_2:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 8, 4, 64]
    // CHECK-SAME:  } inputs([[GROUP_CONVOLUTION]] : tensor<1x16x16x8xf16, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x8x4x64xf16, {order = #NHWC}>

    // CHECK:       return [[OUT_SHAPE_CAST_1]], [[OUT_SHAPE_CAST_2]]
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>
#NCWH = affine_map<(d0, d1, d2, d3) -> (d0, d1, d3, d2)>

!qElemType0 = !quant.uniform<u8:f16, 0.5>
!qElemType1 = !quant.uniform<u8:f16, 0.25>

// CHECK-LABEL: @PropagatePermuteThroughAddWith2QuantCasts
// CHECK-SAME:        [[BLOCK_ARG:%arg0]]: tensor<1x4x8x64x!qElemType0>
func.func @PropagatePermuteThroughAddWith2QuantCasts(%arg0: tensor<1x4x8x64x!qElemType0>)
    -> (tensor<1x8x4x64x!qElemType1, {order = #NHWC}>, tensor<1x8x4x64x!qElemType1, {order = #NHWC}>)  {
    %IN_MEM_PERM = IE.MemPermute(%arg0) {
        dst_order = #NHWC,
        mem_perm = #NCWH
    } : tensor<1x4x8x64x!qElemType0> -> tensor<1x8x4x64x!qElemType0, {order = #NHWC}>

    %IN_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 16, 16, 8]
    } inputs(%IN_MEM_PERM : tensor<1x8x4x64x!qElemType0, {order = #NHWC}>) -> tensor<1x16x16x8x!qElemType0, {order = #NHWC}>

    %ADD = IE.Add(%IN_SHAPE_CAST, %IN_SHAPE_CAST) {
        auto_broadcast = #IE.auto_broadcast_type<NUMPY>
    } : tensor<1x16x16x8x!qElemType0, {order = #NHWC}>, tensor<1x16x16x8x!qElemType0, {order = #NHWC}>
            -> tensor<1x16x16x8x!qElemType0, {order = #NHWC}>

    %OUT_SHAPE_CAST = IE.ShapeCast {
        shape = [1, 8, 4, 64]
    } inputs(%ADD : tensor<1x16x16x8x!qElemType0, {order = #NHWC}>) -> tensor<1x8x4x64x!qElemType0, {order = #NHWC}>

    %OUT_QUANT_CAST_1 = IE.QuantizeCast(%OUT_SHAPE_CAST) {
        dstElemType = !qElemType1
    } : tensor<1x8x4x64x!qElemType0, {order = #NHWC}>
            -> tensor<1x8x4x64x!qElemType1, {order = #NHWC}>

    %OUT_QUANT_CAST_2 = IE.QuantizeCast(%OUT_SHAPE_CAST) {
        dstElemType = !qElemType1
    } : tensor<1x8x4x64x!qElemType0, {order = #NHWC}>
            -> tensor<1x8x4x64x!qElemType1, {order = #NHWC}>

    return %OUT_QUANT_CAST_1, %OUT_QUANT_CAST_2 : tensor<1x8x4x64x!qElemType1, {order = #NHWC}>, tensor<1x8x4x64x!qElemType1, {order = #NHWC}>

    // CHECK:       [[PERMUTE_CAST:%.+]] = IE.PermuteCast([[BLOCK_ARG]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NCHW
    // CHECK-SAME:  } : tensor<1x4x8x64x!qElemType0> -> tensor<1x64x4x8x!qElemType0, {order = #NHWC}>

    // CHECK:       [[IN_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 16, 16, 8]
    // CHECK-SAME:  } inputs([[PERMUTE_CAST]] : tensor<1x64x4x8x!qElemType0, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x16x16x8x!qElemType0, {order = #NHWC}>

    // CHECK:       [[ADD:%.+]] = IE.Add([[IN_SHAPE_CAST]], [[IN_SHAPE_CAST]])

    // CHECK:       [[OUT_SHAPE_CAST:%.+]] = IE.ShapeCast {
    // CHECK-SAME:      shape = [1, 64, 4, 8]
    // CHECK-SAME:  } inputs([[ADD]] : tensor<1x16x16x8x!qElemType0, {order = #NHWC}>)
    // CHECK-SAME:      -> tensor<1x64x4x8x!qElemType0, {order = #NHWC}>

    // CHECK:       [[OUT_MEM_PERMUTE:%.+]] = IE.MemPermute([[OUT_SHAPE_CAST]]) {
    // CHECK-SAME:      dst_order = #NHWC,
    // CHECK-SAME:      mem_perm = #NCWH
    // CHECK-SAME:  } : tensor<1x64x4x8x!qElemType0, {order = #NHWC}> -> tensor<1x8x4x64x!qElemType0, {order = #NHWC}>

    // CHECK:       [[OUT_QUANT_CAST_1:%.+]] = IE.QuantizeCast([[OUT_MEM_PERMUTE]])
    // CHECK:       [[OUT_QUANT_CAST_2:%.+]] = IE.QuantizeCast([[OUT_MEM_PERMUTE]])

    // CHECK:       return [[OUT_QUANT_CAST_1]], [[OUT_QUANT_CAST_2]]
}

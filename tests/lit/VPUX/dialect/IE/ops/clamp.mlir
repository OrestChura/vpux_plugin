//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --canonicalize --split-input-file --init-compiler="vpu-arch=%arch%" %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

// CHECK-LABEL: @FuseClamps1
func.func @FuseClamps1(%arg0: tensor<1x30x30x30xf16>) -> tensor<1x30x30x30xf16> {
    %0 = IE.Clamp(%arg0) {max = 20.000000e+00 : f64, min = 1.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %1 = IE.Clamp(%0) {max = 10.000000e+00 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    return %1 : tensor<1x30x30x30xf16>

    // CHECK:        [[CLAMP:%.*]] = IE.Clamp(%arg0) {max = 1.000000e+01 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK-NEXT:       return [[CLAMP]]
}

// -----

#NHCW = affine_map<(d0, d1, d2, d3) -> (d0, d2, d1, d3)>

// CHECK-LABEL: @FuseClamps2
func.func @FuseClamps2(%arg0: tensor<1x30x30x30xf16>) -> tensor<1x30x30x30xf16> {
    %0 = IE.Clamp(%arg0) {max = 20.000000e+00 : f64, min = 1.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %1 = IE.Clamp(%0) {max = 10.000000e+00 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %2 = IE.Transpose(%1) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %3 = IE.Transpose(%1) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %4 = IE.Add(%2, %3) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x30x30x30xf16>, tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    return %4 : tensor<1x30x30x30xf16>

    // CHECK:        [[CLAMP:%.*]] = IE.Clamp(%arg0) {max = 1.000000e+01 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK-NEXT:       [[TRANSPOSE_1:%.*]] = IE.Transpose([[CLAMP]]) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:        [[TRANSPOSE_2:%.*]] = IE.Transpose(%0) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:        [[ADD:%.*]] = IE.Add([[TRANSPOSE_1]], [[TRANSPOSE_2]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x30x30x30xf16>, tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:       return [[ADD]]
}

// -----

#NHCW = affine_map<(d0, d1, d2, d3) -> (d0, d2, d1, d3)>

// CHECK-LABEL: @ClampsNotFused
func.func @ClampsNotFused(%arg0: tensor<1x30x30x30xf16>) -> tensor<1x30x30x30xf16> {
    %0 = IE.Clamp(%arg0) {max = 20.000000e+00 : f64, min = 1.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %1 = IE.Clamp(%0) {max = 10.000000e+00 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %2 = IE.Transpose(%0) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    %3 = IE.Add(%1, %2) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x30x30x30xf16>, tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    return %3 : tensor<1x30x30x30xf16>

    // CHECK:        [[CLAMP_1:%.*]] = IE.Clamp(%arg0) {max = 2.000000e+01 : f64, min = 1.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:        [[CLAMP_2:%.*]] = IE.Clamp([[CLAMP_1]]) {max = 1.000000e+01 : f64, min = 5.000000e+00 : f64} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK-NEXT:       [[TRANSPOSE:%.*]] = IE.Transpose([[CLAMP_1]]) {order_value = #NHCW} : tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:        [[ADD:%.*]] = IE.Add([[CLAMP_2]], [[TRANSPOSE]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x30x30x30xf16>, tensor<1x30x30x30xf16> -> tensor<1x30x30x30xf16>
    // CHECK:       return [[ADD]]
}

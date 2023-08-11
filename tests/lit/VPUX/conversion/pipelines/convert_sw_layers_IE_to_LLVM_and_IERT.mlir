//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=VPUX37XX" --ShaveCodeGen %s | FileCheck %s

module @SingleLayer {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        IE.DataInfo "input" : tensor<1x1000xf16>
    }
    outputsInfo : {
        IE.DataInfo "cos" : tensor<1x1000xf16>
    }

func.func @main(%arg0: tensor<1x1000xf16>) -> tensor<1x1000xf16> {
    %0 = IE.Cos(%arg0) : tensor<1x1000xf16> -> tensor<1x1000xf16>
    return %0 : tensor<1x1000xf16>
}

}

// CHECK: llvm.func @Cos0(%[[ARG0:.+]]: !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>) -> !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)> {
// CHECK: %[[VAL2:.+]] = llvm.mlir.constant(1 : i64) : i64
// CHECK: %[[VAL3:.+]] = llvm.getelementptr %[[ARG0]][%[[VAL2]]] : (!llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>, i64) -> !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>
// CHECK: %[[VAL4:.+]] = llvm.load %[[VAL3]] : !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>
// CHECK: %[[VAL37:.+]] = "llvm.intr.cos"(%[[VAL36:.+]]) : (f16) -> f16
// CHECK: llvm.return %[[VAL4:.+]] : !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>

// CHECK: IE.CNNNetwork entryPoint : @main inputsInfo : {

// CHECK: func.func @main(%[[ARGB0:.+]]: memref<1x1000xf16>, %arg1: memref<1x1000xf16>) -> memref<1x1000xf16> {
// CHECK: %[[VAL0:.+]] = IERT.GenericReshape inputs(%[[ARGB0:.+]] : memref<1x1000xf16>) -> memref<1x1x1x1000xf16>
// CHECK: %[[VAL1:.+]] = memref.alloc() : memref<1x1x1x1000xf16>
// CHECK: %[[VAL2:.+]] = IERT.PackMemref(%[[VAL0]], %[[VAL1]] : memref<1x1x1x1000xf16>, memref<1x1x1x1000xf16>) -> !IERT.PackedParams
// CHECK: %[[VAL3:.+]] = IERT.ExtendedCall @VPU.SW::@Cos0(%[[VAL2]]) : (!IERT.PackedParams) -> memref<1x1x1x1000xf16>
// CHECK: return %[[VAL5:.+]] : memref<1x1000xf16>

// -----

module @SingleLayer {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        IE.DataInfo "input" : tensor<1x1000xf16>
    }
    outputsInfo : {
        IE.DataInfo "hswish" : tensor<1x1000xf16>
    }

func.func @main(%arg0: tensor<1x1000xf16>) -> tensor<1x1000xf16> {
    %0 = IE.HSwish(%arg0) : tensor<1x1000xf16> -> tensor<1x1000xf16>
    return %0 : tensor<1x1000xf16>
}

}

// CHECK: llvm.func @HSwish0(%[[ARG0:.+]]: !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>) -> !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)> {
// CHECK: %[[VAL2:.+]] = llvm.mlir.constant(1 : i64) : i64
// CHECK: %[[VAL3:.+]] = llvm.getelementptr %[[ARG0]][%[[VAL2]]] : (!llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>, i64) -> !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<
// CHECK: %[[VAL4:.+]] = llvm.load %[[VAL3]] : !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>
// CHECK: %[[VAL49:.+]] = llvm.fdiv %[[VAL48:.+]], %[[VAL44:.+]]  : f16
// CHECK: llvm.return %[[VAL4]] : !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>

// CHECK: IE.CNNNetwork entryPoint : @main inputsInfo : {

// CHECK: func.func @main(%[[ARGB0:.+]]: memref<1x1000xf16>, %arg1: memref<1x1000xf16>) -> memref<1x1000xf16> {
// CHECK: %[[VAL0:.+]] = IERT.GenericReshape inputs(%[[ARGB0]] : memref<1x1000xf16>) -> memref<1x1x1x1000xf16>
// CHECK: %[[VAL1:.+]] = memref.alloc() : memref<1x1x1x1000xf16>
// CHECK: %[[VAL2:.+]] = IERT.PackMemref(%[[VAL0]], %[[VAL1]] : memref<1x1x1x1000xf16>, memref<1x1x1x1000xf16>) -> !IERT.PackedParams
// CHECK: %[[VAL3:.+]] = IERT.ExtendedCall @VPU.SW::@HSwish0(%[[VAL2]]) : (!IERT.PackedParams) -> memref<1x1x1x1000xf16>
// CHECK: return %[[VAL5:.+]] : memref<1x1000xf16>

// -----

module @SingleLayer {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        IE.DataInfo "input" : tensor<1x1000xf16>
    }
    outputsInfo : {
        IE.DataInfo "softmax" : tensor<1x1000xf16>
    }

func.func @main(%arg0: tensor<1x1000xf16>) -> tensor<1x1000xf16> {
    %0 = IE.SoftMax(%arg0) {axisInd = 1} : tensor<1x1000xf16> -> tensor<1x1000xf16>
    return %0 : tensor<1x1000xf16>
}

}

// CHECK: llvm.func @SoftMax0(%[[ARG0:.+]]: !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>) -> !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)> {
// CHECK: %[[VAL16:.+]] = llvm.mlir.constant(1 : i64) : i64
// CHECK: %[[VAL17:.+]] = llvm.getelementptr %[[ARG0]][%[[VAL16]]] : (!llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>, i64) -> !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>
// CHECK: %[[VAL18:.+]] = llvm.load %[[VAL17]] : !llvm.ptr<struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>>
// CHECK: llvm.return %[[VAL18:.+]] : !llvm.struct<(ptr<f16>, ptr<f16>, i64, array<4 x i64>, array<4 x i64>)>

// CHECK: IE.CNNNetwork entryPoint : @main inputsInfo : {

// CHECK: func.func @main(%[[ARGB0:.+]]: memref<1x1000xf16>, %arg1: memref<1x1000xf16>) -> memref<1x1000xf16> {
// CHECK: %[[VAL0:.+]] = IERT.GenericReshape inputs(%[[ARGB0]] : memref<1x1000xf16>) -> memref<1x1x1x1000xf16>
// CHECK: %[[VAL1:.+]] = memref.alloc() : memref<1x1x1x1000xf16>
// CHECK: %[[VAL2:.+]] = IERT.PackMemref(%[[VAL0]], %[[VAL1]] : memref<1x1x1x1000xf16>, memref<1x1x1x1000xf16>) -> !IERT.PackedParams
// CHECK: %[[VAL3:.+]] = IERT.ExtendedCall @VPU.SW::@SoftMax0(%[[VAL2:.+]]) : (!IERT.PackedParams) -> memref<1x1x1x1000xf16>
// CHECK: return %[[VAL5:.+]] : memref<1x1000xf16>

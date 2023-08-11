//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch% compilation-mode=DefaultHW" --convert-layers-to-VPU %s | FileCheck %s
// REQUIRES: arch-VPUX37XX

// CHECK-LABEL: @EmbeddingSegmentsSumWithWeights
func.func @EmbeddingSegmentsSumWithWeights(%arg0: tensor<5x6x4xui8>) -> tensor<7x6x4xui8> {
    %cst = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 2, 3]> : tensor<5xsi32>
    %cst_0 = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 3, 4]> : tensor<5xsi32>
    %cst_1 = const.Declare tensor<5xui8> = dense<[1, 5, 10, 8, 10]> : tensor<5xui8>
    %0 = IE.EmbeddingSegmentsSum(%arg0, %cst, %cst_0, %cst_1) {default_index_value = 0 : i32, num_segments_value = 7 : i32, operand_segment_sizes = dense<[1, 1, 1, 0, 0, 1]> : vector<6xi32>} : tensor<5x6x4xui8>, tensor<5xsi32>, tensor<5xsi32>, tensor<5xui8> -> tensor<7x6x4xui8>
    return %0 : tensor<7x6x4xui8>

    // CHECK-DAG: [[CST:%.+]] = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 2, 3]> : tensor<5xsi32>
    // CHECK-DAG: [[CST0:%.+]] = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 3, 4]> : tensor<5xsi32>
    // CHECK-DAG: [[CST1:%.+]] = const.Declare tensor<5xui8> = dense<[1, 5, 10, 8, 10]> : tensor<5xui8>
    // CHECK: [[VAR0:%.+]] = VPU.EmbeddingSegmentsSum(%arg0, %cst, %cst_0, %cst_1) {default_index_value = 0 : i32, num_segments_value = 7 : i32, operand_segment_sizes = dense<1> : vector<4xi32>} : tensor<5x6x4xui8>, tensor<5xsi32>, tensor<5xsi32>, tensor<5xui8> -> tensor<7x6x4xui8>
    // CHECK: return [[VAR0]] : tensor<7x6x4xui8>
}

// -----

// CHECK-LABEL: @EmbeddingSegmentsSumNoWeights
func.func @EmbeddingSegmentsSumNoWeights(%arg0: tensor<5x6x4xui8>) -> tensor<7x6x4xui8> {
    %cst = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 2, 3]> : tensor<5xsi32>
    %cst_0 = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 3, 4]> : tensor<5xsi32>
    %0 = IE.EmbeddingSegmentsSum(%arg0, %cst, %cst_0) {default_index_value = -1 : i32, num_segments_value = 7 : i32, operand_segment_sizes = dense<[1, 1, 1, 0, 0, 0]> : vector<6xi32>} : tensor<5x6x4xui8>, tensor<5xsi32>, tensor<5xsi32> -> tensor<7x6x4xui8>
    return %0 : tensor<7x6x4xui8>

    // CHECK-DAG: [[CST:%.+]] = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 2, 3]> : tensor<5xsi32>
    // CHECK-DAG: [[CST0:%.+]] = const.Declare tensor<5xsi32> = dense<[0, 1, 2, 3, 4]> : tensor<5xsi32>
    // CHECK-DAG: [[CST1:%.+]] = const.Declare tensor<5xui8> = dense<1> : tensor<5xui8>
    // CHECK: [[VAR0:%.+]] = VPU.EmbeddingSegmentsSum(%arg0, %cst, %cst_0, %cst_1) {default_index_value = -1 : i32, num_segments_value = 7 : i32, operand_segment_sizes = dense<1> : vector<4xi32>} : tensor<5x6x4xui8>, tensor<5xsi32>, tensor<5xsi32>, tensor<5xui8> -> tensor<7x6x4xui8>
    // CHECK: return [[VAR0]] : tensor<7x6x4xui8>
}

//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=VPUX30XX compilation-mode=DefaultHW allow-custom-values=true" %s | FileCheck %s

// CHECK: module @mode attributes {VPU.arch = "VPUX30XX", VPU.compilationMode = "ReferenceSW"}
module @mode attributes {VPU.compilationMode = "ReferenceSW"} {
}

// -----

// CHECK: module @arch attributes {VPU.arch = "VPUX37XX", VPU.compilationMode = "DefaultHW"}
module @arch attributes {VPU.arch = "VPUX37XX"} {
}

// -----

// CHECK: module @executors attributes {VPU.arch = "VPUX30XX", VPU.compilationMode = "DefaultHW"}
module @executors {
    IE.ExecutorResource 2 of @DMA_NN
    IE.ExecutorResource 2 of @NCE at 6.000000e+02 MHz
}

// CHECK-DAG:    IE.ExecutorResource 16 of @SHAVE_UPA
// CHECK-DAG:    IE.ExecutorResource 2 of @DMA_NN
// CHECK-DAG:    IE.ExecutorResource 2 of @NCE at 6.000000e+02 MHz {
// CHECK-DAG:        IE.ExecutorResource 5 of @DPU

// CHECK-DAG:   IE.MemoryResource 825753 bytes of @CMX_NN_FragmentationAware
// CHECK-DAG:   IE.MemoryResource 917504 bytes of @CMX_NN {VPU.bandwidth = 32 : i64, VPU.derateFactor = 1.000000e+00 : f64}
// CHECK-DAG:   IE.MemoryResource 524288000 bytes of @DDR {VPU.bandwidth = 8 : i64, VPU.derateFactor = 6.000000e-01 : f64}

// -----

// CHECK: module @memory attributes {VPU.arch = "VPUX30XX", VPU.compilationMode = "DefaultHW"}
module @memory {
    IE.MemoryResource 5 bytes of @CMX_NN_FragmentationAware
    IE.MemoryResource 10000 bytes of @CMX_NN {VPU.bandwidth = 10 : i64, VPU.derateFactor = 2.0 : f64}
    IE.MemoryResource 500000 bytes of @DDR
}

// CHECK-DAG:    IE.ExecutorResource 16 of @SHAVE_UPA
// CHECK-DAG:    IE.ExecutorResource 1 of @DMA_NN
// CHECK-DAG:    IE.ExecutorResource 4 of @NCE at 7.000000e+02 MHz {
// CHECK-DAG:        IE.ExecutorResource 5 of @DPU

// CHECK-DAG:   IE.MemoryResource 5 bytes of @CMX_NN_FragmentationAware
// CHECK-DAG:   IE.MemoryResource 10000 bytes of @CMX_NN {VPU.bandwidth = 10 : i64, VPU.derateFactor = 2.000000e+00 : f64}
// CHECK-DAG:   IE.MemoryResource 500000 bytes of @DDR {VPU.bandwidth = 8 : i64, VPU.derateFactor = 6.000000e-01 : f64}

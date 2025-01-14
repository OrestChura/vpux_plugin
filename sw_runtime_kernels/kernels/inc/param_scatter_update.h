//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#ifdef __MOVICOMPILE__
#include <moviVectorTypes.h>
#else
typedef fp16 half;
#endif

#include <common_types.h>

#ifdef __cplusplus
namespace sw_params {
#endif

#pragma pack(push, 1)

enum { SCATTER_UPDATE_MAX_SUPPORTED_DIMS = 8 };

struct __attribute__((packed)) ScatterUpdateParams {
    struct MemRefData data;
    struct MemRefData indices;
    struct MemRefData updates;
    struct MemRefData output;
    int64_t axis;
};

#pragma pack(pop)

inline struct BaseKernelParams ToBaseKernelParams(struct ScatterUpdateParams* params) {
    struct BaseKernelParams result;
    result.numInputs = 3;
    result.numOutputs = 1;
#ifdef __cplusplus
    result.inputsOffset = reinterpret_cast<uint8_t*>(&(params->data)) - reinterpret_cast<uint8_t*>(params);
    result.outputsOffset = reinterpret_cast<uint8_t*>(&(params->output)) - reinterpret_cast<uint8_t*>(params);
#else
    result.inputsOffset = (uint8_t*)(&(params->data)) - (uint8_t*)(params);
    result.outputsOffset = (uint8_t*)(&(params->output)) - (uint8_t*)(params);
#endif
    return result;
}

#ifdef __cplusplus
}  // namespace sw_params
#endif

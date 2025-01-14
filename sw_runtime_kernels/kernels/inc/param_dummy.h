//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

//

#ifndef _DUMMY_PARAMS_
#define _DUMMY_PARAMS_

#ifdef __MOVICOMPILE__
#    include <moviVectorTypes.h>
#else
typedef fp16 half;
#endif

#include <common_types.h>

#ifdef __cplusplus
namespace sw_params {
#endif

#pragma pack(push, 1)

struct DummyParams {
    uint64_t numIns;
    uint64_t numOuts;
    struct MemRefData tensors[MAX_KERNEL_INPUTS + MAX_KERNEL_OUTPUTS];
};

#pragma pack(pop)

inline struct BaseKernelParams dummyParamsToBaseKernelParams(struct DummyParams * dummyParams) {
    struct BaseKernelParams result;
    result.numInputs = dummyParams->numIns;
    result.numOutputs = dummyParams->numOuts;
#ifdef  __cplusplus
    result.inputsOffset = reinterpret_cast<uint8_t*>(dummyParams->tensors) - reinterpret_cast<uint8_t*>(dummyParams);
    result.outputsOffset = reinterpret_cast<uint8_t*>(dummyParams->tensors + dummyParams->numIns) - reinterpret_cast<uint8_t*>(dummyParams);
#else
    result.inputsOffset = (uint8_t*)(dummyParams->tensors) - (uint8_t*)(dummyParams);
    result.outputsOffset = (uint8_t*)(dummyParams->tensors + dummyParams->numIns) - (uint8_t*)(dummyParams);
#endif
    return result;
}

#ifdef __cplusplus
}  // namespace sw_params
#endif

#endif  // _DUMMY_PARAMS_

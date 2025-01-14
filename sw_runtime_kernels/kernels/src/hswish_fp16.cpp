//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <param_hswish.h>
#include <dma_shave_nn.h>

#define VECTOR_SIZE 8  // Changes to this should be reflected in the code as well.

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

using namespace sw_params;

namespace nn {
namespace shave_lib {

extern "C" {

void hswish_fp16(uint32_t lParamsAddr) {
    const HSwishParams* lParams = reinterpret_cast<const HSwishParams*>(lParamsAddr);

    half8* __restrict__ p_act_data = reinterpret_cast<half8*>(lParams->input.dataAddr);  // 0x1F000000
    half8* __restrict__ p_act_out = reinterpret_cast<half8*>(lParams->output.dataAddr);  // 0x1F004000
    int32_t* pDims = (int32_t*)(lParams->input.dimsAddr);

    const half8 add_val_3 = 3.0f;
    const half8 max_val_6 = 6.0f;

    int32_t nElements = 1;
    int32_t i = 0;
    for (i = 0; i != lParams->input.numDims; i++) {
        // TODO: check overflow
        nElements *= pDims[i];
    }

    const int numVectors = nElements / VECTOR_SIZE;
    const int remElements = nElements % VECTOR_SIZE;

    for(i = 0; i < numVectors; ++i){
        p_act_out[i] =  __builtin_shave_cmu_clamp0_f16_rr_half8(p_act_data[i] + add_val_3, max_val_6) * p_act_data[i] / max_val_6;
    }

    for(int j = 0; j < remElements; j++){
        p_act_out[numVectors][j] = p_act_data[numVectors][j] * MIN(6.f, MAX(0.f, p_act_data[numVectors][j] + 3.f)) * 0.16666f;
    }
}

}
} // namespace shave_lib
} // namespace nn

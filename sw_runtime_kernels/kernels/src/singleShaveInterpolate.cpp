//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <moviVectorConvert.h>
#include "mvSubspaces.h"

#include <math.h>
#include <moviVectorTypes.h>
#include <moviVectorUtils.h>

#include <param_interpolate.h>
#include <cmath>

using namespace sw_params;
using namespace subspace;

namespace {

struct Dimensions {
    int width;
    int height;
    int channels;
    int strideW;
    int strideH;
    int strideC;
    NDOrder storageOrder;
};

struct InterpolateParamsCMX {
    half* input;
    half* output;

    Dimensions inDims;
    Dimensions outDims;

    InterpolationMethod interpolation_mode;
    InterpolationCoordTransMode coord_transform_mode;
    InterpolationNearestMode nearest_mode;
    uint32_t antialias;
};

__attribute__((always_inline)) int32_t min(int32_t x, int32_t y) {
    return __builtin_shave_cmu_min_i32_rr_int(x, y);
}

__attribute__((always_inline)) int32_t max(int32_t x, int32_t y) {
    return __builtin_shave_cmu_max_i32_rr_int(x, y);
}

__attribute__((always_inline)) static int round_positive(float x) {
    return static_cast<int>(x + 0.5f);
}

union halfU {
    half8 h8;
    short8 s8;
    half4 h4[2];
};

#define USE_FAST_ROUND

#ifdef USE_FAST_ROUND
#define ROUND(x) round_positive(x)
#else
#define ROUND(x) roundf(x)
#endif

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

float alphaCoordinateTransform(InterpolationCoordTransMode coordTransMode, float rotateValue) {
    if (coordTransMode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN)
        return rotateValue / 2.0f;
    else if (coordTransMode == InterpolationCoordTransMode::PYTORCH_HALF_PIXEL)
        return rotateValue / 2.0f - 0.5f;
    else if (coordTransMode == InterpolationCoordTransMode::ASYMMETRIC)
        return 0.0f;
    else
        return rotateValue / 2.0f - .5f;
}

float coordinateTransform(InterpolationCoordTransMode coordTransMode, int x_resized, float x_scale, int length_resized,
                          int length_original) {
    switch (coordTransMode) {
    case InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN: {
        return (x_resized + 0.5f) * x_scale;
    }
    case InterpolationCoordTransMode::PYTORCH_HALF_PIXEL: {
        if (length_resized > 1) {
            return (x_resized + 0.5f) * x_scale - 0.5f;
        }
        return 0.0f;
    }
    case InterpolationCoordTransMode::ASYMMETRIC: {
        return x_resized * x_scale;
    }
    case InterpolationCoordTransMode::HALF_PIXEL: {
        return ((x_resized + 0.5f) * x_scale) - 0.5f;
    }
    case InterpolationCoordTransMode::ALIGN_CORNERS: {
        if (length_resized == 1) {
            return 0.0f;
        }
        return x_resized * static_cast<float>(length_original - 1) / (length_resized - 1);
    }
    }
    return x_resized * x_scale;
}

float round_prefer_floor(float x) {
    float temp;
    float frac = std::modf(x, &temp);
    return (frac == 0.5f) ? std::floor(x) : ROUND(x);
}

// workaround for moviSim errors described in [E#24996]
float round_floor(float x) {
    return std::floor(x);
}

float round_ceil(float x) {
    return std::ceil(x);
}

float round_roundf(float x) {
    return std::roundf(x);
}

void interpolationHWC_nearest(const half* psrc, half* pdst, int OW, int IW, int OH, int IH, int C, float rw,
                              float alpha, float (*roundingFunc)(float), InterpolationCoordTransMode coord) {
    UNUSED(OH);
    UNUSED(IH);
    for (int w = 0; w < OW; w++) {
        float fw = (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * w + alpha;
        int iw = min(static_cast<int>(roundingFunc(fw)), IW - 1);

        for (int c = 0; c < C / 32 * 32; c += 32) {
            half8 result0 = *((half8*)(psrc + iw * C + c + 0));
            half8 result1 = *((half8*)(psrc + iw * C + c + 8));
            half8 result2 = *((half8*)(psrc + iw * C + c + 16));
            half8 result3 = *((half8*)(psrc + iw * C + c + 24));

            *((half8*)(pdst + w * C + c + 0)) = result0;
            *((half8*)(pdst + w * C + c + 8)) = result1;
            *((half8*)(pdst + w * C + c + 16)) = result2;
            *((half8*)(pdst + w * C + c + 24)) = result3;
        }
        for (int c = C / 32 * 32; c < C; c++) {
            pdst[w * C + c] = psrc[iw * C + c];
        }
    }
}

void interpolationCHW_nearest(const half* psrc, half* pdst, int OW, int IW, int OH, int IH, int C, float rw,
                              float alpha, float (*roundingFunc)(float), InterpolationCoordTransMode coord) {
    for (int w = 0; w < OW / 8; w++) {
        float fw0 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 0) + alpha;
        float fw1 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 1) + alpha;
        float fw2 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 2) + alpha;
        float fw3 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 3) + alpha;

        float fw4 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 4) + alpha;
        float fw5 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 5) + alpha;
        float fw6 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 6) + alpha;
        float fw7 =
                (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * (w * 8 + 7) + alpha;

        const int iw0 = min(static_cast<int>(roundingFunc(fw0)), IW - 1);
        const int iw1 = min(static_cast<int>(roundingFunc(fw1)), IW - 1);
        const int iw2 = min(static_cast<int>(roundingFunc(fw2)), IW - 1);
        const int iw3 = min(static_cast<int>(roundingFunc(fw3)), IW - 1);

        const int iw4 = min(static_cast<int>(roundingFunc(fw4)), IW - 1);
        const int iw5 = min(static_cast<int>(roundingFunc(fw5)), IW - 1);
        const int iw6 = min(static_cast<int>(roundingFunc(fw6)), IW - 1);
        const int iw7 = min(static_cast<int>(roundingFunc(fw7)), IW - 1);

        for (int c = 0; c < C; c++) {
            half8 val = {
                    *((half*)(psrc + c * IH * IW + iw0)), *((half*)(psrc + c * IH * IW + iw1)),
                    *((half*)(psrc + c * IH * IW + iw2)), *((half*)(psrc + c * IH * IW + iw3)),
                    *((half*)(psrc + c * IH * IW + iw4)), *((half*)(psrc + c * IH * IW + iw5)),
                    *((half*)(psrc + c * IH * IW + iw6)), *((half*)(psrc + c * IH * IW + iw7)),
            };
            *((half8*)(pdst + c * OH * OW + w * 8)) = val;
        }
    }

    for (int w = OW / 8 * 8; w < OW; w++) {
        float fw = (coord == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1) ? 0 : rw * w + alpha;
        const int iw = min(static_cast<int>(roundingFunc(fw)), IW - 1);

        for (int c = 0; c < C; c++) {
            *((half*)(pdst + c * OW + w)) = *((half*)(psrc + c * IW + iw));
        }
    }
}

void interpolationCHW(half* psrc0, half* psrc1, half* pdst, half* w_lambda, ushort* ind, half* row_buf, int OW,
                      int oStride, int IW, int iStride, int C, float h_lambda0, float h_lambda1,
                      int ow_last_interpolate) {
    half8 hl0 = mvuConvert_half(h_lambda0);
    half8 hl1 = mvuConvert_half(h_lambda1);

    for (int c = 0; c < C; c++) {
        for (int w = 0; w < IW; w += 8) {
            half8 vsrc0 = *((half8*)(psrc0 + c * iStride + w));
            half8 vsrc1 = *((half8*)(psrc1 + c * iStride + w));

            half8 result = hl1 * vsrc0 + hl0 * vsrc1;

            *((half8*)(row_buf + w)) = result;
        }

        for (int w = 0; w <= ow_last_interpolate; w += 8) {
            half8 s0 = {row_buf[ind[w + 0]], row_buf[ind[w + 1]], row_buf[ind[w + 2]], row_buf[ind[w + 3]],
                        row_buf[ind[w + 4]], row_buf[ind[w + 5]], row_buf[ind[w + 6]], row_buf[ind[w + 7]]};
            half8 s1 = {row_buf[ind[w + 0] + 1], row_buf[ind[w + 1] + 1], row_buf[ind[w + 2] + 1],
                        row_buf[ind[w + 3] + 1], row_buf[ind[w + 4] + 1], row_buf[ind[w + 5] + 1],
                        row_buf[ind[w + 6] + 1], row_buf[ind[w + 7] + 1]};

            half8 wl0 = *((half8*)(w_lambda + w));
            half8 wl1 = 1.f - wl0;

            half8 result = wl1 * s0 + wl0 * s1;

            *((half8*)(pdst + c * oStride + w)) = result;
        }
        // calc the last
        for (int w = ow_last_interpolate + 1; w < OW; w++) {
            pdst[(c + 0) * oStride + w] = row_buf[IW - 1];
        }
    }
}

void optKerInterpCHW4x(half* __restrict psrc0, half* __restrict psrc1, half* __restrict pdst, int OW, int oStride,
                       int IW, int iStride, int C, int ow_last_interpolate) {
    const half hl0[4] = {0.00, 0.25, 0.50, 0.75};
    const half hl1[4] = {1.00, 0.75, 0.50, 0.25};  // 1 - hl0
    alignas(8) half row_buf[4][IW + 16];

    for (int c = 0; c < C; c++) {
        // Vertical interpolation
        for (int w = 0; w < IW; w += 8 * 2) {
            half8 vsrc0A = *((half8*)(psrc0 + c * iStride + w + 0));
            half8 vsrc1A = *((half8*)(psrc1 + c * iStride + w + 0));

            half8 vsrc0B = *((half8*)(psrc0 + c * iStride + w + 8));
            half8 vsrc1B = *((half8*)(psrc1 + c * iStride + w + 8));

            *((half8*)(row_buf[0] + w + 0)) = hl1[0] * vsrc0A + hl0[0] * vsrc1A;
            *((half8*)(row_buf[1] + w + 0)) = hl1[1] * vsrc0A + hl0[1] * vsrc1A;
            *((half8*)(row_buf[2] + w + 0)) = hl1[2] * vsrc0A + hl0[2] * vsrc1A;
            *((half8*)(row_buf[3] + w + 0)) = hl1[3] * vsrc0A + hl0[3] * vsrc1A;

            *((half8*)(row_buf[0] + w + 8)) = hl1[0] * vsrc0B + hl0[0] * vsrc1B;
            *((half8*)(row_buf[1] + w + 8)) = hl1[1] * vsrc0B + hl0[1] * vsrc1B;
            *((half8*)(row_buf[2] + w + 8)) = hl1[2] * vsrc0B + hl0[2] * vsrc1B;
            *((half8*)(row_buf[3] + w + 8)) = hl1[3] * vsrc0B + hl0[3] * vsrc1B;
        }

        // Horizontal interpolation
        const half8 wl0 = {0.00, 0.25, 0.50, 0.75, 0.00, 0.25, 0.50, 0.75};
        const half8 wl1 = 1.f - wl0;

        for (int r = 0; r < 4; r++) {
#pragma clang loop unroll_count(5)
            for (int w = 0; w <= ow_last_interpolate; w += 8 * 2) {
                half p0 = row_buf[r][w / 4 + 0];
                half p1 = row_buf[r][w / 4 + 1];
                half p2 = row_buf[r][w / 4 + 2];
                half p3 = row_buf[r][w / 4 + 3];
                half p4 = row_buf[r][w / 4 + 4];

                half4 h0 = {p0, p0, p0, p0};
                half4 h1 = {p1, p1, p1, p1};
                half4 h2 = {p2, p2, p2, p2};

                half4 h3 = {p3, p3, p3, p3};
                half4 h4 = {p4, p4, p4, p4};

                halfU s0, s1;
                s0.h4[0] = h0;
                s0.h4[1] = h1;  // => {p0,p0,p0,p0,p1,p1,p1,p1}
                s1.h4[0] = h1;
                s1.h4[1] = h2;  // => {p1,p1,p1,p1,p2,p2,p2,p2}

                halfU _s0, _s1;
                _s0.h4[0] = h2;
                _s0.h4[1] = h3;  // => {p2,p2,p2,p2,p3,p3,p3,p3}
                _s1.h4[0] = h3;
                _s1.h4[1] = h4;  // => {p3,p3,p3,p3,p4,p4,p4,p4}

                half8 result_A = wl1 * s0.h8 + wl0 * s1.h8;
                half8 result_B = wl1 * _s0.h8 + wl0 * _s1.h8;

                *((half8*)(pdst + r * OW + c * oStride + w + 0)) = result_A;
                *((half8*)(pdst + r * OW + c * oStride + w + 8)) = result_B;
            }
        }

        // calc the last
        for (int w = ow_last_interpolate + 1; w < OW; w++) {
            pdst[c * oStride + 0 * OW + w] = row_buf[0][IW - 1];
            pdst[c * oStride + 1 * OW + w] = row_buf[1][IW - 1];
            pdst[c * oStride + 2 * OW + w] = row_buf[2][IW - 1];
            pdst[c * oStride + 3 * OW + w] = row_buf[3][IW - 1];
        }
    }
}

// Optimization for 4X, InterpolationMethod::LINEAR_ONNX, InterpolationCoordTransMode::ASYMMETRIC
void optInterpCHW4x(InterpolateParamsCMX* params, half* psrc, half* pdst, int ow_last_interpolate) {
    const int IW = params->inDims.width;
    const int IH = params->inDims.height;
    const int OW = params->outDims.width;
    const int OH = params->outDims.height;
    const int C = params->inDims.channels;
    const int in_stride = params->inDims.strideH;
    const int out_stride = params->outDims.strideH;

    int iy0, iy1;  // input line indices

    for (int y = 0; y < OH; y += 4) {
        iy0 = y / 4;
        iy1 = iy0 < (IH - 1) ? iy0 + 1 : iy0;

        optKerInterpCHW4x(psrc + iy0 * in_stride, psrc + iy1 * in_stride, pdst + y * out_stride, OW, OW * OH, IW,
                          IW * IH, C, ow_last_interpolate);
    }
}

void optKerInterpCHW2x(half* __restrict psrc0, half* __restrict psrc1, half* __restrict pdst, int OW, int oStride,
                       int IW, int iStride, int C, half h_lambda0, half h_lambda1, int ow_last_interpolate) {
    half8 hl0 = h_lambda0;
    half8 hl1 = h_lambda1;
    alignas(8) half row_buf[IW];

    for (int c = 0; c < C; c++) {
        // Vertical interpolation
        for (int w = 0; w < IW; w += 8 * 2) {
            half8 vsrc0A = *((half8*)(psrc0 + c * iStride + w));
            half8 vsrc1A = *((half8*)(psrc1 + c * iStride + w));

            half8 vsrc0B = *((half8*)(psrc0 + c * iStride + w + 8));
            half8 vsrc1B = *((half8*)(psrc1 + c * iStride + w + 8));

            half8 resultA = hl1 * vsrc0A + hl0 * vsrc1A;
            half8 resultB = hl1 * vsrc0B + hl0 * vsrc1B;

            *((half8*)(row_buf + w + 0)) = resultA;
            *((half8*)(row_buf + w + 8)) = resultB;
        }

        // Horizontal interpolation
        const half8 wl0 = {0.0, 0.5, 0.0, 0.5, 0.0, 0.5, 0.0, 0.5};
        const half8 wl1 = 1.f - wl0;

#pragma clang loop unroll_count(5)
        for (int w = 0; w <= ow_last_interpolate; w += 8) {
            half p0 = row_buf[w / 2 + 0];
            half p1 = row_buf[w / 2 + 1];
            half p2 = row_buf[w / 2 + 2];
            half p3 = row_buf[w / 2 + 3];
            half p4 = row_buf[w / 2 + 4];
            half8 s0 = {p0, p0, p1, p1, p2, p2, p3, p3};
            halfU s1;  //= {p1,p1,p2,p2,p3,p3,p4,p4};
            s1.h8 = s0;
            s1.s8 = __builtin_shave_cmu_shriv_x16_rr(s1.s8, *(short*)&p4);
            s1.s8 = __builtin_shave_cmu_shriv_x16_rr(s1.s8, *(short*)&p4);

            half8 result = wl1 * s0 + wl0 * s1.h8;
            *((half8*)(pdst + c * oStride + w)) = result;
        };

        // calc the last
        for (int w = ow_last_interpolate + 1; w < OW; w++) {
            pdst[(c + 0) * oStride + w] = row_buf[IW - 1];
        }
    }
}

// Optimization for 2X, InterpolationMethod::LINEAR_ONNX, InterpolationCoordTransMode::ASYMMETRIC
void optInterpCHW2x(InterpolateParamsCMX* params, half* psrc, half* pdst, int ow_last_interpolate) {
    const int IW = params->inDims.width;
    const int IH = params->inDims.height;
    const int OW = params->outDims.width;
    const int OH = params->outDims.height;
    const int C = params->inDims.channels;
    const int in_stride = params->inDims.strideH;
    const int out_stride = params->outDims.strideH;

    const half2 h_lambda0 = {0.00, 0.50};
    const half2 h_lambda1 = 1.0f - h_lambda0;
    int iy0, iy1;  // input line indices

    for (int y = 0; y < OH; y++) {
        half lambda0 = h_lambda0[y % 2];
        half lambda1 = h_lambda1[y % 2];

        iy0 = y / 2;
        iy1 = iy0 < (IH - 1) ? iy0 + 1 : iy0;

        optKerInterpCHW2x(psrc + iy0 * in_stride, psrc + iy1 * in_stride, pdst + y * out_stride, OW, OW * OH, IW,
                          IW * IH, C, lambda0, lambda1, ow_last_interpolate);
    }
}

void nnResample(InterpolateParamsCMX* params) {
    const int IW = params->inDims.width;
    const int IH = params->inDims.height;
    const int OW = params->outDims.width;
    const int OH = params->outDims.height;
    const int C = params->inDims.channels;

    const bool inOrderCHW = (params->inDims.storageOrder == ND_CHW || params->inDims.storageOrder == ND_NCHW ||
                             params->inDims.storageOrder == 0x321 || params->inDims.storageOrder == 0x4321);
    const bool outOrderCHW = (params->inDims.storageOrder == ND_CHW || params->inDims.storageOrder == ND_NCHW ||
                              params->outDims.storageOrder == 0x321 || params->outDims.storageOrder == 0x4321);

    const float rh = static_cast<float>(IH) / static_cast<float>(OH);
    const float rw = static_cast<float>(IW) / static_cast<float>(OW);

    const auto interpolation_function = [&] {
        if (inOrderCHW) {
            return interpolationCHW_nearest;
        } else {
            return interpolationHWC_nearest;
        }
    }();

    float (*roundingFunc)(float);
    switch (params->nearest_mode) {
    case InterpolationNearestMode::FLOOR: {
        roundingFunc = round_floor;
        break;
    }
    case InterpolationNearestMode::CEIL: {
        roundingFunc = round_ceil;
        break;
    }
    case InterpolationNearestMode::ROUND_PREFER_FLOOR: {
        roundingFunc = round_prefer_floor;
        break;
    }
    case InterpolationNearestMode::SIMPLE: {
        roundingFunc = round_floor;
        break;
    }
    default: {
        roundingFunc = round_roundf;
        break;
    }
    }

    for (int y = 0; y < OH; y++) {
        float alpha = alphaCoordinateTransform(params->coord_transform_mode, rw);
        float fh = (params->coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0
                           : rh * y + alpha;
        int ih = min(static_cast<int>(roundingFunc(fh)), IH - 1);

        int inOffset = ih * params->inDims.strideH;
        int outOffset = y * params->outDims.strideH;

        alpha = alphaCoordinateTransform(params->coord_transform_mode, rh);
        interpolation_function(params->input + inOffset, params->output + outOffset, OW, IW, OH, IH, C, rw, alpha,
                               roundingFunc, params->coord_transform_mode);
    }
}

void interpolationCHW_1x1(const half* psrc, half* pdst, int OW, int OH, int C, int stride) {
    for (int c = 0; c < C; c++) {
        half8 vsrc = psrc[c * stride];
        half ssrc = psrc[c * stride];
        int w;

        for (w = 0; w < OW - 7; w += 8) {
            *((half8*)(pdst + c * OH * OW + w)) = vsrc;
        }
        for (; w < OW; w++) {
            *((half*)(pdst + c * OH * OW + w)) = ssrc;
        }
    }
}

void interpolationHWC(InterpolateParamsCMX* params, half* psrc0, half* psrc1, half* pdst, int OW, int IW, int C,
                      float rw, float h_lambda0, float h_lambda1) {
    int in_x1[OW];
    int in_x2[OW];
    float dx1[OW];
    float dx2[OW];

    for (int w = 0; w < OW; w++) {
        float fw = (params->coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1)
                           ? 0.0f
                           : coordinateTransform(params->coord_transform_mode, w, rw, OW, IW);
        int iw0 = static_cast<int>(fw);
        int iw1 = (iw0 < IW - 1) ? iw0 + 1 : iw0;

        if (params->interpolation_mode == InterpolationMethod::LINEAR_ONNX) {
            fw = MIN(fw, IW - 1);
            in_x1[w] = MIN(static_cast<int>(fw), IW - 1);
            in_x2[w] = MIN(fw + 1, IW - 1);

            dx1[w] = (in_x1[w] == in_x2[w]) ? 0.5f : fabsf(fw - in_x1[w]);
            dx2[w] = (in_x1[w] == in_x2[w]) ? 0.5f : fabsf(fw - in_x2[w]);
        }

        float w_lambda0 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? fw - iw0 : dx1[w];
        float w_lambda1 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? 1.0f - w_lambda0 : dx2[w];

        half8 hl0 = mvuConvert_half(h_lambda0);
        half8 hl1 = mvuConvert_half(h_lambda1);
        half8 wl0 = mvuConvert_half(w_lambda0);
        half8 wl1 = mvuConvert_half(w_lambda1);

        int c = 0;
        for (; c < C - 31; c += 32) {
            half8 src0_00 = *((half8*)(psrc0 + iw0 * C + c));
            half8 src0_01 = *((half8*)(psrc0 + iw1 * C + c));
            half8 src0_10 = *((half8*)(psrc1 + iw0 * C + c));
            half8 src0_11 = *((half8*)(psrc1 + iw1 * C + c));

            half8 src1_00 = *((half8*)(psrc0 + iw0 * C + c + 8));
            half8 src1_01 = *((half8*)(psrc0 + iw1 * C + c + 8));
            half8 src1_10 = *((half8*)(psrc1 + iw0 * C + c + 8));
            half8 src1_11 = *((half8*)(psrc1 + iw1 * C + c + 8));

            half8 src2_00 = *((half8*)(psrc0 + iw0 * C + c + 16));
            half8 src2_01 = *((half8*)(psrc0 + iw1 * C + c + 16));
            half8 src2_10 = *((half8*)(psrc1 + iw0 * C + c + 16));
            half8 src2_11 = *((half8*)(psrc1 + iw1 * C + c + 16));

            half8 src3_00 = *((half8*)(psrc0 + iw0 * C + c + 24));
            half8 src3_01 = *((half8*)(psrc0 + iw1 * C + c + 24));
            half8 src3_10 = *((half8*)(psrc1 + iw0 * C + c + 24));
            half8 src3_11 = *((half8*)(psrc1 + iw1 * C + c + 24));

            half8 result0 = hl1 * (wl1 * src0_00 + wl0 * src0_01) + hl0 * (wl1 * src0_10 + wl0 * src0_11);

            half8 result1 = hl1 * (wl1 * src1_00 + wl0 * src1_01) + hl0 * (wl1 * src1_10 + wl0 * src1_11);

            half8 result2 = hl1 * (wl1 * src2_00 + wl0 * src2_01) + hl0 * (wl1 * src2_10 + wl0 * src2_11);

            half8 result3 = hl1 * (wl1 * src3_00 + wl0 * src3_01) + hl0 * (wl1 * src3_10 + wl0 * src3_11);

            *((half8*)(pdst + w * C + c + 0)) = result0;
            *((half8*)(pdst + w * C + c + 8)) = result1;
            *((half8*)(pdst + w * C + c + 16)) = result2;
            *((half8*)(pdst + w * C + c + 24)) = result3;
        }
        for (; c < C - 7; c += 8) {
            half8 src0_00 = *((half8*)(psrc0 + iw0 * C + c));
            half8 src0_01 = *((half8*)(psrc0 + iw1 * C + c));
            half8 src0_10 = *((half8*)(psrc1 + iw0 * C + c));
            half8 src0_11 = *((half8*)(psrc1 + iw1 * C + c));

            half8 result0 = hl1 * (wl1 * src0_00 + wl0 * src0_01) + hl0 * (wl1 * src0_10 + wl0 * src0_11);

            *((half8*)(pdst + w * C + c + 0)) = result0;
        }
        for (; c < C; c++) {
            half src0_00 = *((half*)(psrc0 + iw0 * C + c));
            half src0_01 = *((half*)(psrc0 + iw1 * C + c));
            half src0_10 = *((half*)(psrc1 + iw0 * C + c));
            half src0_11 = *((half*)(psrc1 + iw1 * C + c));

            half result0 =
                    hl1[0] * (wl1[0] * src0_00 + wl0[0] * src0_01) + hl0[0] * (wl1[0] * src0_10 + wl0[0] * src0_11);

            *((half*)(pdst + w * C + c)) = result0;
        }
    }
}

void interpHWC(InterpolateParamsCMX* params) {
    const int IW = params->inDims.width;
    const int IH = params->inDims.height;
    const int OW = params->outDims.width;
    const int OH = params->outDims.height;
    const int C = params->inDims.channels;

    // middle strides
    const int in_stride = params->inDims.strideW;
    const int out_stride = params->outDims.strideW;

    bool align_corners = params->coord_transform_mode == InterpolationCoordTransMode::ALIGN_CORNERS;
    InterpolationCoordTransMode coord_transform_mode = params->coord_transform_mode;

    half* psrc = params->input;
    half* pdst = params->output;

    int y0 = 0;
    int y1 = OH;

    int in_y1 = 0;
    int in_y2 = 0;

    float dy1 = 0;
    float dy2 = 0;

    const float rh = (OH > 1 && align_corners) ? static_cast<float>(IH - 1) / (OH - 1) : static_cast<float>(IH) / OH;
    const float rw = (OW > 1 && align_corners) ? static_cast<float>(IW - 1) / (OW - 1) : static_cast<float>(IW) / OW;

    for (int y = y0; y < y1; y++) {
        float fh = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, y, rh, OH, IH);
        int ih0 = static_cast<int>(fh);
        int ih1 = (ih0 < IH - 1) ? ih0 + 1 : ih0;

        if (params->interpolation_mode == InterpolationMethod::LINEAR_ONNX) {
            fh = MAX(0, MIN(fh, IH - 1));
            in_y1 = MAX(0, MIN(static_cast<int>(fh), IH - 1));
            in_y2 = MIN(in_y1 + 1, IH - 1);

            dy1 = (in_y1 == in_y2) ? 0.5f : fabsf(fh - in_y1);
            dy2 = (in_y1 == in_y2) ? 0.5f : fabsf(fh - in_y2);
        }

        float h_lambda0 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? fh - ih0 : dy1;
        float h_lambda1 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? 1.0f - h_lambda0 : dy2;

        const int inOffset0 = ih0 * IW * in_stride;
        const int inOffset1 = ih1 * IW * in_stride;
        const int outOffset = y * OW * out_stride;

        interpolationHWC(params, psrc + inOffset0, psrc + inOffset1, pdst + outOffset, OW, IW, C, rw, h_lambda0,
                         h_lambda1);
    }
}

void interpCHW(InterpolateParamsCMX* params) {
    const int IW = params->inDims.width;
    const int IH = params->inDims.height;
    const int OW = params->outDims.width;
    const int OH = params->outDims.height;
    const int C = params->inDims.channels;

    // middle strides
    const int in_stride = params->inDims.strideH;
    const int out_stride = params->outDims.strideH;

    bool align_corners = params->coord_transform_mode == InterpolationCoordTransMode::ALIGN_CORNERS;
    InterpolationCoordTransMode coord_transform_mode = params->coord_transform_mode;

    int MW = MAX(IW, OW);
    half lambda[MW + 7];
    ushort ind[MW + 7];
    half row_buf[MW + 7];

    half* psrc = params->input;
    half* pdst = params->output;

    int y0 = 0;
    int y1 = OH;

    const float rh_0 = (OH > 1 && align_corners) ? (IH - 1) : IH;
    const float rh_1 = (OH > 1 && align_corners) ? (OH - 1) : OH;

    const float rw_0 = (OW > 1 && align_corners) ? (IW - 1) : IW;
    const float rw_1 = (OW > 1 && align_corners) ? (OW - 1) : OW;

    const float rh = static_cast<float>(rh_0) / rh_1;
    const float rw = static_cast<float>(rw_0) / rw_1;

    /* ow_last_interpolate: max{oh: oh is interger, floor(oh * rw) <= IW - 2} */
    const float rw_inv = static_cast<float>(rw_1) / rw_0;
    int ow_last_interpolate = ceil((IW - 1) * rw_inv) - 1;

    // Optimizations for specific cases
    if ((params->interpolation_mode == InterpolationMethod::LINEAR_ONNX) &&
        (coord_transform_mode == InterpolationCoordTransMode::ASYMMETRIC)) {
        int sel = 0;
        void (*optFunc[])(InterpolateParamsCMX*, half*, half*, int) = {
                nullptr,         // 0
                optInterpCHW4x,  // 1
                optInterpCHW2x,  // 2
        };

        // kernel does 16 pix on output horizontal direction
        // kernel allocates a few lines of IW pixels on stack, so can deal with limited IW value
        if (((OW % 16) == 0) && (IW <= 128)) {
            if ((OW == IW * 4) && (OH == IH * 4))
                sel = 1;
            else if ((OW == IW * 2) && (OH == IH * 2))
                sel = 2;
        }

        if (sel) {
            optFunc[sel](params, psrc, pdst, ow_last_interpolate);
            return;
        }
    }

    float in_x1 = 0;
    float in_x2 = 0;
    float dx1 = 0;

    for (int w = 0; w < OW; w++) {
        float fw = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OW <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, w, rw, OW, IW);
        int iw = static_cast<int>(fw);
        float wl = fw - iw;

        if (params->interpolation_mode == InterpolationMethod::LINEAR_ONNX) {
            float in_x = MIN(fw, IW - 1);
            in_x1 = MIN(static_cast<int>(in_x), IW - 1);
            in_x2 = MIN(in_x1 + 1, IW - 1);
            dx1 = (in_x1 != in_x2) ? fabsf(fw - in_x1) : 0.5f;

            iw = in_x1;
            wl = dx1;
        }
        ind[w] = iw;
        __asm volatile("nop");  // workaround for E52287
        lambda[w] = mvuConvert_half(wl);
    }
    float in_y1 = 0;
    float in_y2 = 0;

    float dy1 = 0;
    float dy2 = 0;

    float min_fh = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, y0, rh, OH, IH);
    //  minimal h coordinate in input tensor which are necessary to calculate the [y0,y1) range of output tensor
    int min_ih0 = static_cast<int>(min_fh);

    float max_fh = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, y1 - 1, rh, OH, IH);
    int max_ih0 = static_cast<int>(max_fh);
    //  maximal h coordinate in input tensor which are necessary to calculate the [y0,y1) range of output tensor
    int max_ih1 = (max_ih0 < IH - 1) ? max_ih0 + 1 : max_ih0;

    for (int y = y0; y < y1; y++) {
        float in_y = 0;
        if (params->interpolation_mode == InterpolationMethod::LINEAR_ONNX) {
            in_y = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, y, rh, OH, IH);
            in_y = MAX(0, MIN(in_y, IH - 1));
            in_y1 = MAX(0, MIN(static_cast<int>(in_y), IH - 1));
            in_y2 = MIN(in_y1 + 1, IH - 1);
            dy1 = (in_y1 != in_y2) ? fabsf(in_y - in_y1) : 0.5f;
            dy2 = (in_y1 != in_y2) ? fabsf(in_y - in_y2) : 0.5f;
        }
        float fh = (coord_transform_mode == InterpolationCoordTransMode::TF_HALF_PIXEL_FOR_NN && OH <= 1)
                           ? 0.0f
                           : coordinateTransform(coord_transform_mode, y, rh, OH, IH);
        int ih0 = static_cast<int>(fh);
        int ih1 = (ih0 < IH - 1) ? ih0 + 1 : ih0;

        float h_lambda0 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? fh - ih0 : dy1;
        float h_lambda1 = (params->interpolation_mode != InterpolationMethod::LINEAR_ONNX) ? 1.0f - h_lambda0 : dy2;

        int inOffset0 = ih0 * in_stride;
        int inOffset1 = ih1 * in_stride;
        int outOffset = y * out_stride;

        interpolationCHW(psrc + inOffset0, psrc + inOffset1, pdst + outOffset, lambda, ind, row_buf, OW, OH * OW, IW,
                         IH * IW, C, h_lambda0, h_lambda1, ow_last_interpolate);
    }
}

}  // namespace

namespace nn {
namespace shave_lib {

extern "C" {
void singleShaveInterpolate(uint32_t lParams) {
    const InterpolateParams* layerParams = reinterpret_cast<const InterpolateParams*>(lParams);

    half* p_act_data = (half*)(layerParams->input.dataAddr);
    half* p_act_out = (half*)(layerParams->output.dataAddr);

    InterpolateParamsCMX paramsCMX;
    InterpolateParamsCMX* sp = &paramsCMX;

    int32_t* iDims = (int32_t*)(layerParams->input.dimsAddr);
    int32_t* oDims = (int32_t*)(layerParams->output.dimsAddr);
    int64_t* iPStrides = (int64_t*)(layerParams->input.stridesAddr);
    int64_t* oPStrides = (int64_t*)(layerParams->output.stridesAddr);

    sp->input = reinterpret_cast<half*>(p_act_data);
    sp->output = reinterpret_cast<half*>(p_act_out);

    sp->interpolation_mode = layerParams->interpolation_mode;
    sp->coord_transform_mode = layerParams->coord_transform_mode;
    sp->nearest_mode = layerParams->nearest_mode;
    sp->antialias = (uint32_t)layerParams->antialias;

    sp->inDims.storageOrder = layerParams->input.dimsOrder;
    sp->outDims.storageOrder = layerParams->output.dimsOrder;
    s32 ndims = layerParams->input.numDims;
    int indiceW, indiceH, indiceC;

    // specifying dimsOrder by hex value is a temporary workaround because inverted
    // dimension orders are not yet defined
    // [E#39088]
    if (sp->inDims.storageOrder == ND_CHW || sp->inDims.storageOrder == ND_NCHW || sp->inDims.storageOrder == 0x321 ||
        sp->inDims.storageOrder == 0x4321) {
        indiceW = 0;
        indiceH = 1;
        indiceC = 2;
    } else {
        indiceC = 0;
        indiceW = 1;
        indiceH = 2;
    }

    sp->inDims.channels = iDims[indiceC];
    sp->inDims.height = iDims[indiceH];
    sp->inDims.width = iDims[indiceW];
    sp->inDims.strideC = iPStrides[indiceC] / (CHAR_BIT * sizeof(half));
    sp->inDims.strideH = iPStrides[indiceH] / (CHAR_BIT * sizeof(half));
    sp->inDims.strideW = iPStrides[indiceW] / (CHAR_BIT * sizeof(half));

    sp->outDims.channels = oDims[indiceC];
    sp->outDims.height = oDims[indiceH];
    sp->outDims.width = oDims[indiceW];
    sp->outDims.strideC = oPStrides[indiceC] / (CHAR_BIT * sizeof(half));
    sp->outDims.strideH = oPStrides[indiceH] / (CHAR_BIT * sizeof(half));
    sp->outDims.strideW = oPStrides[indiceW] / (CHAR_BIT * sizeof(half));

    if (sp->interpolation_mode == InterpolationMethod::NEAREST) {
        if (!sp->antialias) {
            nnResample(sp);
        } else {
            // Not enabled in Inference Engine
        }
    } else {
        if (sp->inDims.storageOrder == ND_CHW || sp->inDims.storageOrder == ND_NCHW ||
            sp->inDims.storageOrder == 0x321 || sp->inDims.storageOrder == 0x4321) {
            interpCHW(sp);
        } else {
            interpHWC(sp);
        }
    }
}

}  // namespace shave_lib
}  // namespace shave_lib
}  // namespace nn

//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

//

#include <custom_cpp_tests.h>
#include <cmath>
#include "mvSubspaces.h"

__attribute__((aligned(1024)))
#include "sk.singleShaveMVN.3720xx.text.xdat"

#include "param_mvn.h"

#define F_EPS 0x3727c5ac  // Hex representation of 10^(-5)f

namespace ICV_TESTS_NAMESPACE(ICV_TESTS_PASTE2(ICV_TEST_SUITE_NAME, MVN)) {

    const bool save_to_file = false;
    const bool customData = true;

    static constexpr std::initializer_list<SingleTest> mvn_test_list{
            {{9, 5, 17},
             {9, 5, 17},
             orderHWC,
             {/*across_channels*/ 0, /*normalize_variance*/ 0, /*eps*/ F_EPS, sw_params::Location::NN_CMX}},
            {{9, 5, 17},
             {9, 5, 17},
             orderHWC,
             {/*across_channels*/ 0, /*normalize_variance*/ 1, /*eps*/ F_EPS, sw_params::Location::NN_CMX}},
            {{9, 5, 17},
             {9, 5, 17},
             orderCHW,
             {/*across_channels*/ 0, /*normalize_variance*/ 0, /*eps*/ F_EPS, sw_params::Location::NN_CMX}},
            {{9, 5, 17},
             {9, 5, 17},
             orderCHW,
             {/*across_channels*/ 0, /*normalize_variance*/ 1, /*eps*/ F_EPS, sw_params::Location::NN_CMX}},
    };

    class CustomCppMvnTest : public CustomCppTests<fp16> {
    public:
        explicit CustomCppMvnTest(): m_testsLoop(mvn_test_list, "test") {
        }
        virtual ~CustomCppMvnTest() {
        }

    protected:
        const char* suiteName() const override {
            return "CustomCppMVNTest";
        }
        void userLoops() override {
            addLoop(m_testsLoop);
        }

        void initData() override {
            sw_params::BaseKernelParams emptyParamData;
            m_params = {nullptr, emptyParamData, 0, 0xFFFFFFFF, 0, MAX_LOCAL_PARAMS};

            CustomCppTests<fp16>::initData();
            const SingleTest* test = m_currentTest;
            m_mvnParams = reinterpret_cast<sw_params::MvnParams*>(paramContainer);
            *m_mvnParams = sw_params::MvnParams();
            m_params.paramData = reinterpret_cast<uint32_t*>(paramContainer);
            m_params.paramDataLen = sizeof(sw_params::MvnParams);
            m_requiredTensorLocation = static_cast<sw_params::Location>(test->customLayerParams.layerParams[3]);
            m_params.baseParamData = sw_params::ToBaseKernelParams(m_mvnParams);

            uint32_t acrossChannels = test->customLayerParams.layerParams[0];
            uint32_t normalize = test->customLayerParams.layerParams[1];
            float *eps = (float*)(&test->customLayerParams.layerParams[2]);

            m_mvnParams->acrossChannels = (uint64_t)acrossChannels;
            m_mvnParams->normalize = (uint64_t)normalize;
            m_mvnParams->eps = *eps;

            m_params.kernel = reinterpret_cast<uint32_t>(sk_singleShaveMVN_3720xx_text);
        }

        void initTestCase() override {
            m_currentTest = &m_testsLoop.value();
            m_test_threshold = 0.001f;
        }

        void generateInputData() override {
            // set random seed
            rand_seed();
            u64 ticks_for_seed = rtems_clock_get_uptime_nanoseconds();
            srand(ticks_for_seed);

            // input
            m_inputTensor.forEach(false, [&](const MemoryDims& indices) {
                float tmp = float(rand() % 1000) / 100 - 5.0f;
                m_inputTensor.at(indices) = f32Tof16(tmp);
            });
        }
        void generateReferenceData() override {
            const auto& dims = m_inputTensor.tensorDims();
            const bool normalize_variance = m_mvnParams->normalize;
            const bool across_channels = m_mvnParams->acrossChannels;
            const float epsilon = m_mvnParams->eps;
            int b = 0;

            if (across_channels) {
                float sum = 0.f;
                float sqsum = 0.f;

                for (int c = 0; c < dims.channels; c++) {
                    for (int h = 0; h < dims.height; h++) {
                        for (int w = 0; w < dims.width; w++) {
                            sum += f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b)));
                            sqsum += f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b))) *
                                     f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b)));
                        }
                    }
                }
                float mean = sum / (dims.channels * dims.height * dims.width);

                float variance = sqsum / (dims.channels * dims.height * dims.width);
                variance = variance - mean * mean;
                variance = variance < 0 ? 1.f : variance;
                variance = sqrtf(variance) + epsilon;

                for (int c = 0; c < dims.channels; c++) {
                    for (int h = 0; h < dims.height; h++) {
                        for (int w = 0; w < dims.width; w++) {
                            m_referenceOutputTensor.at(TensorDims(w, h, c, b)) =
                                    f32Tof16(f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b))) - mean);
                            if (normalize_variance) {
                                m_referenceOutputTensor.at(TensorDims(w, h, c, b)) =
                                        f32Tof16(f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b))) / variance);
                            }
                        }
                    }
                }
            } else {  // across_channels == false
                for (int c = 0; c < dims.channels; c++) {
                    float sum = 0.f;
                    float sqsum = 0.f;

                    for (int h = 0; h < dims.height; h++) {
                        for (int w = 0; w < dims.width; w++) {
                            sum += f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b)));
                            sqsum += f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b))) *
                                     f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b)));
                        }
                    }
                    float mean = sum / (dims.height * dims.width);

                    float variance = sqsum / (dims.height * dims.width);
                    variance = variance - mean * mean;
                    variance = variance < 0 ? 1.f : variance;
                    variance = sqrtf(variance) + epsilon;

                    for (int h = 0; h < dims.height; h++) {
                        for (int w = 0; w < dims.width; w++) {
                            m_referenceOutputTensor.at(TensorDims(w, h, c, b)) =
                                    f32Tof16(f16Tof32(m_inputTensor.at(TensorDims(w, h, c, b))) - mean);
                            if (normalize_variance) {
                                m_referenceOutputTensor.at(TensorDims(w, h, c, b)) = f32Tof16(
                                        f16Tof32(m_referenceOutputTensor.at(TensorDims(w, h, c, b))) / variance);
                            }
                        }
                    }
                }
            }
        }

        virtual bool checkResult() override {
            m_outputTensor.confirmBufferData();

            // save output data
            if (save_to_file) {
                saveMemoryToFile(reinterpret_cast<u32>(m_inputTensor.buffer()), m_inputTensor.bufferSize(),
                                 "inMyriad.bin");

                saveMemoryToFile(reinterpret_cast<u32>(m_outputTensor.buffer()), m_outputTensor.bufferSize(),
                                 "outMyriad.bin");

                saveMemoryToFile(reinterpret_cast<u32>(m_referenceOutputTensor.buffer()),
                                 m_referenceOutputTensor.bufferSize(), "refOutMyriad.bin");
            }

            bool threshold_test_failed = false;

            m_outputTensor.forEach(false, [&](const MemoryDims& indices) {
                float value = f16Tof32(m_outputTensor.at(indices));
                float gt_value = f16Tof32(m_referenceOutputTensor.at(indices));
                float abs_diff = fabs(value - gt_value);
                bool differ = !bool(abs_diff <= m_test_threshold);

                threshold_test_failed |= differ;

                if (differ && GlobalData::doPrintDiffs) {
                    const TensorDims ti = m_outputTensor.toTensor(indices);
                    printf("DIFF HWC [%d:%d:%d] %f %f %f\n", ti.height, ti.width, ti.channels, value, gt_value,
                           abs_diff);
                }
            });

            return !threshold_test_failed;
        }

    private:
        ListIterator<SingleTest> m_testsLoop;

        int m_axis;
        sw_params::MvnParams* m_mvnParams;
    };

    ICV_TESTS_REGISTER_SUITE(CustomCppMvnTest)
}  // namespace )

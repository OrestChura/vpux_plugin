//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

//

#include <custom_cpp_tests.h>
#include "mvSubspaces.h"

__attribute__((aligned(1024)))
#include "sk.log_fp16.3720xx.text.xdat"

#include "param_log.h"

namespace ICV_TESTS_NAMESPACE(ICV_TESTS_PASTE2(ICV_TEST_SUITE_NAME, Log)) {
    static constexpr std::initializer_list<SingleTest> log_test_list{
            {{2, 2, 2}, {2, 2, 2}, orderZYX, {sw_params::Location::NN_CMX}},
            {{1, 1, 20}, {1, 1, 20}, orderZYX, {sw_params::Location::NN_CMX}},
            {{1000, 1, 1}, {1000, 1, 1}, orderZYX, {sw_params::Location::NN_CMX}}};

    class CustomCppLogTest : public CustomCppTests<fp16> {
    public:
        explicit CustomCppLogTest(): m_testsLoop(log_test_list, "test") {
        }
        virtual ~CustomCppLogTest() {
        }

    protected:
        const char* suiteName() const override {
            return "CustomCppLogTest";
        }
        void userLoops() override {
            addLoop(m_testsLoop);
        }

        void initData() override {
            sw_params::BaseKernelParams emptyParamData;
            m_params = {nullptr, emptyParamData, 0, 0xFFFFFFFF, 0, MAX_LOCAL_PARAMS};
            CustomCppTests<fp16>::initData();
            const SingleTest* test = m_currentTest;
            int32_t ind[subspace::MAX_DIMS] = {0};
            subspace::orderToIndices((t_D8StorageOrder)(test->storageOrder), ind);
            m_logParams = reinterpret_cast<sw_params::LogParams*>(paramContainer);
            *m_logParams = sw_params::LogParams();
            m_params.paramData = reinterpret_cast<uint32_t*>(paramContainer);
            m_params.paramDataLen = sizeof(sw_params::LogParams);
            m_requiredTensorLocation = static_cast<sw_params::Location>(test->customLayerParams.layerParams[0]);
            m_params.baseParamData = sw_params::ToBaseKernelParams(m_logParams);
            m_params.kernel = reinterpret_cast<uint32_t>(sk_log_fp16_3720xx_text);
        }

        void initTestCase() override {
            m_currentTest = &m_testsLoop.value();
            m_test_threshold = 0.005f;
        }

        void generateInputData() override {
            rand_seed();

            // input
            const int ndims = m_inputTensor.ndims();
            m_inputTensor.forEach(false, [&](const MemoryDims& indices) {
                int tmp = 1;
                for (int i = 0; i < ndims; ++i)
                    tmp *= (3 + (indices.dims[i] % 13));
                float tmp2 = 0.001f + (tmp % 33);
                m_inputTensor.at(indices) = f32Tof16(tmp2);
            });
        }
        void generateReferenceData() override {
            m_inputTensor.forEach(false, [&](const MemoryDims& indices) {
                float val = f16Tof32(m_inputTensor.at(indices));
                float ref = (float)log(val);
                m_referenceOutputTensor.at(indices) = f32Tof16(ref);
            });
        }
        virtual bool checkResult() override {
            m_outputTensor.confirmBufferData();

            // save output data
            if (m_save_to_file) {
                saveMemoryToFile(reinterpret_cast<u32>(m_outputTensor.buffer()), m_outputTensor.bufferSize(),
                                 "outMyriad.bin");
            }

            // no need to remap memory indices between tensors
            mvTensorAssert(m_outputTensor.storageOrder() == m_inputTensor.storageOrder());
            mvTensorAssert(m_outputTensor.storageOrder() == m_referenceOutputTensor.storageOrder());

            bool threshold_test_failed = false;
            float max_abs_diff = 0.0;
            float max_rel_diff = 0.0;

            m_outputTensor.forEach(false, [&](const MemoryDims& indices) {
                float value = f16Tof32(m_outputTensor.at(indices));
                float gt_value = f16Tof32(m_referenceOutputTensor.at(indices));

                float abs_diff = fabs(value - gt_value);
                float rel_diff = gt_value != 0.0 ? fabs(abs_diff / gt_value) : abs_diff;
                max_abs_diff = std::max(max_abs_diff, abs_diff);
                max_rel_diff = std::max(max_rel_diff, rel_diff);

                float abs_threshold = (fabs(gt_value) * m_test_threshold);
                bool differ = bool(!(abs_diff <= abs_threshold));

                threshold_test_failed |= differ;

                if (differ && GlobalData::doPrintDiffs) {
                    char indices_str[64];
                    printf("DIFF [%s] %f %f %f abs_diff: %f rel_diff: %f\n",
                           m_outputTensor.indicesToString(indices, indices_str), f16Tof32(m_inputTensor.at(indices)),
                           value, gt_value, abs_diff, rel_diff);
                }
            });

            if (GlobalData::doPrintDiffMax)
                printf("MAX DIFF ABS=%f REL=%f\n", max_abs_diff, max_rel_diff);

            return !threshold_test_failed;
        }

    private:
        ListIterator<SingleTest> m_testsLoop;

        sw_params::LogParams* m_logParams;
    };

    ICV_TESTS_REGISTER_SUITE(CustomCppLogTest)
}  // namespace )

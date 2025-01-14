//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

//

#include <custom_cpp_tests.h>
#include "mvSubspaces.h"
#include <algorithm>
#include <common_types.h>

#include "param_dummy.h"

using namespace sw_params;

__attribute__((aligned(1024)))
#include "sk.dummy.3720xx.text.xdat"

namespace ICV_TESTS_NAMESPACE(ICV_TESTS_PASTE2(ICV_TEST_SUITE_NAME, Dummy))
{
static constexpr std::initializer_list<SingleTest> dummy_test_list
{
    {{2, 2, 2}, {2, 2, 2}, orderZYX, {/*{{224, 1, 1}, {1, 128, 1}, {0, 0, 0}, 3, 0}, */{2 /*ins*/, 2 /*outs*/, sw_params::Location::UPA_CMX /*mem type*/,}}},
    {{2, 1, 1}, {1, 1, 1}, orderZYX, {/*{{224, 1, 1}, {1, 128, 1}, {0, 0, 0}, 3, 0}, */{3 /*ins*/, 1 /*outs*/, sw_params::Location::UPA_CMX /*mem type*/,}}},
    {{2, 1, 1}, {1, 1, 1}, orderZYX, {/*{{224, 1, 1}, {1, 128, 1}, {0, 0, 0}, 3, 0}, */{3 /*ins*/, 4 /*outs*/, sw_params::Location::UPA_CMX /*mem type*/,}}},
    {{1, 1, 1}, {5, 5, 5}, orderZYX, {/*{{224, 1, 1}, {1, 128, 1}, {0, 0, 0}, 3, 0}, */{8 /*ins*/, 4 /*outs*/, sw_params::Location::UPA_CMX /*mem type*/,}}},
    {{1, 1, 1}, {5, 5, 5}, orderZYX, {/*{{224, 1, 1}, {1, 128, 1}, {0, 0, 0}, 3, 0}, */{1 /*ins*/, 4 /*outs*/, sw_params::Location::UPA_CMX /*mem type*/,}}},
};

class CustomCppDummyTest: public CustomCppTests<fp16> {
public:
    explicit CustomCppDummyTest(): m_testsLoop(dummy_test_list, "test") {}
    virtual ~CustomCppDummyTest() {}
protected:
    const char* suiteName() const override {
        return "CustomCppDummyTest";
    }
    void userLoops() override {
        addLoop(m_testsLoop);
    }

    void initData() override {
        sw_params::BaseKernelParams emptyParamData;
        m_params = {nullptr, emptyParamData, 0, 0xFFFFFFFF, 0, MAX_LOCAL_PARAMS};

        initTestCase();
        const SingleTest* test = m_currentTest;

        const Dims& inputDims = m_currentTest->inputDims;
        const Dims& outputDims = m_currentTest->outputDims;
        const StorageOrder& storageOrder = m_currentTest->storageOrder;

        const TensorDims dims3In(inputDims.begin()[0], inputDims.begin()[1], inputDims.begin()[2], 1);
        const TensorDims dims3Out(outputDims.begin()[0], outputDims.begin()[1], outputDims.begin()[2], 1);

        m_ins = std::min<int>(test->customLayerParams.layerParams[0], MAX_KERNEL_INPUTS);
        m_outs = std::min<int>(test->customLayerParams.layerParams[1], MAX_KERNEL_OUTPUTS);

        for (int i = 0; i < m_ins; i++) {
            m_inputTensors[i].init(storageOrder, dims3In);
            allocBuffer(m_inputTensors[i]);
        }
        for (int i = 0; i < m_outs; i++) {
            m_outputTensors[i].init(storageOrder, dims3Out);
            allocBuffer(m_outputTensors[i]);
        }

        m_dummyParams = reinterpret_cast<sw_params::DummyParams *>(paramContainer);
        *m_dummyParams = sw_params::DummyParams();
        m_dummyParams->numIns = m_ins;
        m_dummyParams->numOuts = m_outs;
        m_params.paramData = reinterpret_cast<uint32_t*>(paramContainer);
        m_params.paramDataLen = sizeof(sw_params::DummyParams);
        m_requiredTensorLocation = static_cast<sw_params::Location>(test->customLayerParams.layerParams[2]);
        m_params.baseParamData = sw_params::dummyParamsToBaseKernelParams(m_dummyParams);

        m_params.kernel  = reinterpret_cast<uint32_t>(sk_dummy_3720xx_text);
    }

    void formatTestParams(char* str, int maxLength) const override {
        snprintf_append(str, maxLength, "Dummy: inputs: %u; outputs: %u", m_ins, m_outs);
    }

    void initParserRunner() override {
        initMyriadResources();

        static_assert(std::is_base_of<Op, CustomCpp>());
        CustomCpp* customCppOp = static_cast<CustomCpp*>(m_op);
        OpTensor buff;
        for (int i = 0; i < m_ins; i++) {
            m_inputTensors[i].exportToBuffer(buff);
            customCppOp->addInputBuffer(buff, m_requiredTensorLocation);
        }
        for (int i = 0; i < m_outs; i++) {
            m_outputTensors[i].exportToBuffer(buff);
            customCppOp->addOutputBuffer(buff, m_requiredTensorLocation);
        }

        customCppOp->ops = *getParams();
    }

    void resetOutputData() override {
        for (int i = 0; i < m_outs; i++) {
            resetTensorBuffer(m_outputTensors[i]);
        }
    }

    void initTestCase() override {
        m_currentTest = &m_testsLoop.value();
    }

    void generateInputData() override {
        // set random seed
        u64 ticks_for_seed = rtems_clock_get_uptime_nanoseconds();
        srand(ticks_for_seed);

        for (int i = 0; i < m_ins; i++) {
            float tmp = float(rand() % 1000) / 100 - 5.0f;
            (m_inputTensors[i].data())[0] = f32Tof16(tmp);
            m_refs[i] = ((uint8_t*)(m_inputTensors[i].data()))[0];
        }
    }

    void generateReferenceData() override {}

    virtual bool checkResult() override {
        bool test_failed = false;
        for (int i = 0; i < m_outs; i++) {
            m_outputTensors[i].confirmBufferData();
            int nDims = m_outputTensors[i].ndims();
            int toCheck = (nDims > 0) ? std::min<int>(m_ins,
                    m_outputTensors[i].memoryDims().dims[nDims - 1] * m_outputTensors[i].memorySteps().dims[nDims - 1]) : 0;
            for (int j = 0; j < toCheck; j++) {
                uint8_t value = ((uint8_t*)(m_outputTensors[i].data()))[j];
                uint8_t gt_value = m_refs[j];
                bool differ = (gt_value != value);
                test_failed |= differ;
                if (differ && GlobalData::doPrintDiffs) {
                    printf("DIFF: output[%d], value[%d] %u %u\n", i, j, (unsigned)value, (unsigned)gt_value);
                }
            }
        }
        return !test_failed;
    }

private:
    ListIterator<SingleTest> m_testsLoop;

    // Additional buffer to avoid convertion back and forth
    int m_ins;
    int m_outs;
    uint8_t m_refs[MAX_KERNEL_INPUTS];
    Tensor<fp16> m_inputTensors[MAX_KERNEL_INPUTS];
    Tensor<fp16> m_outputTensors[MAX_KERNEL_OUTPUTS];
    sw_params::DummyParams * m_dummyParams;
};

ICV_TESTS_REGISTER_SUITE(CustomCppDummyTest);
}

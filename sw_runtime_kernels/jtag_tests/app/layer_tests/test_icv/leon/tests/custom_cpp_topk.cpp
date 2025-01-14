#include <custom_cpp_tests.h>
#include <cmath>
#include <random>
#include "mvSubspaces.h"

__attribute__((aligned(1024)))
#include "sk.single_shave_topk.3720xx.text.xdat"

#define USE_SEED_VALUE 0xbdd1cb13  // defined to use this value as random seed

#include "param_topk.h"
namespace ICV_TESTS_NAMESPACE(ICV_TESTS_PASTE2(ICV_TEST_SUITE_NAME, TopK)) {
    typedef int32_t Index;
    typedef t_D8StorageOrder StorageOrder;

    static constexpr std::initializer_list<SingleTest> topk_test_list{
            {{4, 4, 4}, {4, 4, 2}, orderZYX, {{2, 2, 0, 2, sw_params::Location::NN_CMX}}},
            {{2, 3, 2}, {1, 3, 2}, orderZYX, {{1, 0, 0, 2, sw_params::Location::NN_CMX}}},
            {{2, 3, 2}, {2, 3, 1}, orderZYX, {{1, 2, 0, 2, sw_params::Location::NN_CMX}}},
            {{2, 3, 2}, {1, 3, 2}, orderZYX, {{1, 0, 1, 2, sw_params::Location::NN_CMX}}},
            {{2, 3, 2}, {2, 3, 1}, orderZYX, {{1, 2, 1, 2, sw_params::Location::NN_CMX}}},
            {{3, 3, 2}, {1, 3, 2}, orderZYX, {{1, 0, 1, 2, sw_params::Location::NN_CMX}}},
            {{2, 3, 2}, {2, 2, 2}, orderZYX, {{2, 1, 1, 2, sw_params::Location::NN_CMX}}},
#ifdef CONFIG_RUN_LARGE_TESTS
            {{11, 10, 10}, {11, 10, 1}, orderZYX, {{1, 2, 0, 2, sw_params::Location::NN_CMX}}},
            {{11, 10, 10}, {2, 10, 10}, orderZYX, {{2, 0, 0, 2, sw_params::Location::NN_CMX}}},
            {{11, 20, 10}, {11, 2, 10}, orderZYX, {{2, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{11, 20, 10}, {11, 20, 2}, orderZYX, {{2, 2, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 40, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 80, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 120, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 160, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 240, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 300, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 320, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 513, 10}, {21, 1, 10}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
            {{21, 513, 20}, {21, 1, 20}, orderZYX, {{1, 1, 0, 2, sw_params::Location::NN_CMX}}},
#endif
            // {{21, 513, 80}, {21, 1, 80}, orderZYX, {{1, 1, 0, 2,sw_params::Location::NN_CMX}}}, //FAILED, CMX:1737120 Byte, sizes are too large for moviSim
    };

    // pair of (value, index) used in sorting
    typedef std::pair<fp16, Index> Pair;

    // comparison function comp(a,b) should return T if a precedes b
    typedef std::function<bool(const Pair&, const Pair&)> CompareFunction;

    static bool compareIndices(const Pair& a, const Pair& b) {
        const Index aIndex = a.second;
        const Index bIndex = b.second;

        if (aIndex < bIndex)
            return true;
        if (aIndex > bIndex)
            return false;

        return true;
    }

    static bool compareValuesMax(const Pair& a, const Pair& b) {
        const float aValue = f16Tof32(a.first);
        const float bValue = f16Tof32(b.first);

        if (!(aValue <= bValue))
            return true;
        if (!(aValue >= bValue))
            return false;

        return compareIndices(a, b);
    }

    static bool compareValuesMin(const Pair& a, const Pair& b) {
        const float aValue = f16Tof32(a.first);
        const float bValue = f16Tof32(b.first);

        if (!(aValue >= bValue))
            return true;
        if (!(aValue <= bValue))
            return false;

        return compareIndices(a, b);
    }

    class CustomCppTopKTest : public CustomCppTests<fp16> {
    public:
        explicit CustomCppTopKTest(): m_testsLoop(topk_test_list, "test") {
        }
        virtual ~CustomCppTopKTest() {
        }

    protected:
        const char* suiteName() const override {
            return "CustomCppTopKTest";
        }
        void userLoops() override {
            addLoop(m_testsLoop);
        }

        void initData() override {
            sw_params::BaseKernelParams emptyParamData;
            m_params = {nullptr, emptyParamData, 0, 0xFFFFFFFF, 0, MAX_LOCAL_PARAMS};

            CustomCppTests<fp16>::initData();

            const SingleTest* test = m_currentTest;
            int32_t ind[subspace::MAX_DIMS] = {
                    0,
            };
            subspace::orderToIndices((t_D8StorageOrder)(test->storageOrder), ind);
            m_k = static_cast<int64_t>(test->customLayerParams.layerParams[0]);
            m_axis = ind[test->customLayerParams.layerParams[1]];
            m_mode = static_cast<int64_t>(test->customLayerParams.layerParams[2]);
            m_sort = static_cast<int64_t>(test->customLayerParams.layerParams[3]);
            m_TopKParams = reinterpret_cast<sw_params::TopKParams*>(paramContainer);
            *m_TopKParams = sw_params::TopKParams();
            m_TopKParams->axis = (int64_t)m_axis;
            m_TopKParams->mode = m_mode;
            m_TopKParams->sort = m_sort;
            m_params.paramData = reinterpret_cast<uint32_t*>(paramContainer);
            m_params.paramDataLen = sizeof(sw_params::TopKParams);
            m_requiredTensorLocation = static_cast<sw_params::Location>(test->customLayerParams.layerParams[4]);
            m_params.baseParamData = sw_params::ToBaseKernelParams(m_TopKParams);

            const Dims& inputDims = m_currentTest->inputDims;
            const Dims& outputDims = m_currentTest->outputDims;
            const StorageOrder& storageOrder = m_currentTest->storageOrder;
            const TensorDims dims3In(inputDims.begin()[0], inputDims.begin()[1], inputDims.begin()[2], 1);
            const TensorDims dims1In(1, 1, 1, 1);
            const TensorDims dims3Out(outputDims.begin()[0], outputDims.begin()[1], outputDims.begin()[2], 1);

            m_inputTensor.init(storageOrder, dims3In);
            m_kTensor.init(storageOrder, dims1In);
            m_outputValueTensor.init(storageOrder, dims3Out);
            m_outputIndexTensor.init(storageOrder, dims3Out);
            m_referenceOutputValueTensor.init(storageOrder, dims3Out);
            m_referenceOutputIndexTensor.init(storageOrder, dims3Out);

            allocBuffer(m_inputTensor);
            allocBuffer(m_kTensor);
            allocBuffer(m_outputValueTensor);
            allocBuffer(m_outputIndexTensor);
            allocBuffer(m_referenceOutputValueTensor);
            allocBuffer(m_referenceOutputIndexTensor);
        }

        void initTestCase() override {
            m_currentTest = &m_testsLoop.value();
            m_test_threshold = 0.01f;
        }

        void initParserRunner() override {
            initMyriadResources();

            static_assert(std::is_base_of<Op, CustomCpp>());
            CustomCpp* customCppOp = static_cast<CustomCpp*>(m_op);

            OpTensor inBuff;
            m_inputTensor.exportToBuffer(inBuff);
            customCppOp->addInputBuffer(inBuff, m_requiredTensorLocation);

            OpTensor kBuff;
            m_kTensor.exportToBuffer(kBuff);
            customCppOp->addInputBuffer(kBuff, m_requiredTensorLocation);

            OpTensor valueBuff;
            m_outputValueTensor.exportToBuffer(valueBuff);
            customCppOp->addOutputBuffer(valueBuff, m_requiredTensorLocation);

            OpTensor indexBuff;
            m_outputIndexTensor.exportToBuffer(indexBuff);
            customCppOp->addOutputBuffer(indexBuff, m_requiredTensorLocation);

            customCppOp->ops = *getParams();
        }

        void resetOutputData() override {
            resetTensorBuffer(m_outputValueTensor);
            resetTensorBuffer(m_outputIndexTensor);
        }

        void generateInputData() override {
            m_params.kernel = reinterpret_cast<uint32_t>(sk_single_shave_topk_3720xx_text);

            // input values
            const u32 range = m_inputTensor.fullSize() + 3;
            const int total = m_inputTensor.dataSize();
            float scale = std::min(total, 50000) / float(total); // prevent FP16 overflow

            m_inputTensor.forEach(false, [&](const MemoryDims& indices) {
                int index = m_inputTensor.index(indices);
                u32 index1 = u32( (u64(index) * index + 17) % range );
                float val1 = scale * float(index1*1 + range/3) / float(range);
                m_inputTensor.at(indices) = f32Tof16(val1);
            });

            // k value
            auto kPtr = m_kTensor.data();
            kPtr[0] = m_k;
        }

        void generateReferenceData() override {
            const int64_t k = m_k;
            const int64_t mode = m_mode;
            const int32_t ndims = m_inputTensor.ndims();
            const int64_t axis = m_axis;

            MemoryDims dims = m_inputTensor.memoryDims();
            const int32_t n = dims.dims[axis];
            dims.dims[axis] = 1;

            mvTensorAssert(k <= n);
            mvTensorAssert(m_inputTensor.storageOrder() == m_referenceOutputValueTensor.storageOrder());
            mvTensorAssert(m_inputTensor.storageOrder() == m_referenceOutputIndexTensor.storageOrder());

            const int32_t inputValuesAxisStep = m_inputTensor.memorySteps().dims[axis];

            const int32_t refValuesAxisStep = m_referenceOutputValueTensor.memorySteps().dims[axis];
            const int32_t refIndicesAxisStep = m_referenceOutputIndexTensor.memorySteps().dims[axis];

            std::vector<Pair> temp(n);
            CompareFunction compareValues = modeComparison(mode);

            dims.forEach(ndims, dims.dims, [&](const MemoryDims& id) {
                const auto inputValuesData = &m_inputTensor.at(id);

                for (int32_t i = 0; i < n; ++i) {
                    temp[i] = Pair(inputValuesData[i * inputValuesAxisStep], i);
                }

                std::partial_sort(temp.begin(), temp.begin() + k, temp.begin() + n, compareValues);
                std::sort(temp.begin(), temp.begin() + k, compareIndices);

                auto refValuesData = &m_referenceOutputValueTensor.at(id);
                auto refIndicesData = &m_referenceOutputIndexTensor.at(id);
                for (int32_t i = 0; i < k; ++i) {
                    const auto& t = temp[i];
                    refValuesData[i * refValuesAxisStep] = t.first;
                    refIndicesData[i * refIndicesAxisStep] = t.second;
                }
            });
        }

        static CompareFunction modeComparison(int32_t mode) {
            switch (mode) {
            case 0 /*max*/:
                return compareValuesMax;
            case 1 /*min*/:
                return compareValuesMin;
            default:
                mvTensorAssert(false);
                return nullptr;
            }
        }

        virtual bool checkResult() override {
            bool test_failed = false;

            m_outputValueTensor.confirmBufferData();
            m_outputIndexTensor.confirmBufferData();
            m_referenceOutputValueTensor.confirmBufferData();
            m_referenceOutputIndexTensor.confirmBufferData();

            m_referenceOutputValueTensor.forEach(true, [this, &test_failed](const MemoryDims& indices) {
                const float gt_value = f16Tof32(m_referenceOutputValueTensor.at(indices));
                float value = f16Tof32(m_outputValueTensor.at(indices));
                float abs_diff = fabs(value - gt_value);
                bool value_differ = !bool(abs_diff <= m_test_threshold);
                const Index gt_index = m_referenceOutputIndexTensor.at(indices);
                const Index out_index = m_outputIndexTensor.at(indices);
                const bool index_differ = (out_index != gt_index);
                const bool differ = value_differ || index_differ;
                test_failed = test_failed || differ;
            });
            return !test_failed;
        }

    private:
        ListIterator<SingleTest> m_testsLoop;
        int64_t m_k;
        int64_t m_axis;
        int64_t m_mode;
        int64_t m_sort;

        sw_params::TopKParams* m_TopKParams;

        Tensor<fp16> m_inputTensor;
        Tensor<int32_t> m_kTensor;
        Tensor<fp16> m_outputValueTensor;
        Tensor<int32_t> m_outputIndexTensor;
        Tensor<fp16> m_referenceOutputValueTensor;
        Tensor<int32_t> m_referenceOutputIndexTensor;
    };

    ICV_TESTS_REGISTER_SUITE(CustomCppTopKTest)
}  // namespace

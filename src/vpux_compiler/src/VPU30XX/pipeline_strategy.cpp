//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/al/config/common.hpp"
#include "vpux/al/config/compiler.hpp"

#include "vpux/compiler/VPU30XX/pipeline_strategy.hpp"
#include "vpux/compiler/VPU30XX/pipelines.hpp"

#include "vpux/compiler/conversion.hpp"
#include "vpux/compiler/options_mapper.hpp"

#include "vpux/compiler/dialect/VPU/attributes.hpp"
#include "vpux/compiler/pipelines.hpp"

#include "vpux/compiler/dialect/VPU/passes.hpp"

using namespace vpux;

//
// PipelineStrategy30XX::buildPipeline
//

void PipelineStrategy30XX::buildPipeline(mlir::PassManager& pm, const Config& config, mlir::TimingScope& rootTiming,
                                         Logger log, const PrecisionInfo& prcInfo) {
    auto buildTiming = rootTiming.nest("Build compilation pipeline");

    const auto archKind = getArchKind(config);
    const auto compilationMode = getCompilationMode(config);
    const auto enableProfiling = config.get<PERF_COUNT>();
    const auto numOfDPUGroups = getNumberOfDPUGroups(config);
    const auto numOfDMAPorts = getNumberOfDMAEngines(config);
    const auto ddrHeapSize = getDDRHeapSize(config);

    pm.addPass(VPU::createInitCompilerPass(archKind, compilationMode, numOfDPUGroups, numOfDMAPorts, ddrHeapSize,
                                           log.nest()));

    if (compilationMode == VPU::CompilationMode::ReferenceSW) {
        const auto options = ReferenceSWOptions30XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;
        if (config.get<PLATFORM>() == InferenceEngine::VPUXConfigParams::VPUXPlatform::EMULATOR) {
            buildEMUReferenceSWModePipeline(pm, *options, log.nest());
        } else {
            buildReferenceSWModePipeline(pm, *options, log.nest());
        }
    } else if (compilationMode == VPU::CompilationMode::ReferenceHW) {
        const auto options = ReferenceHWOptions30XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;
        if (config.get<PLATFORM>() == InferenceEngine::VPUXConfigParams::VPUXPlatform::EMULATOR) {
            buildEMUReferenceHWModePipeline(pm, *options, log.nest());
        } else {
            buildReferenceHWModePipeline(pm, *options, log.nest());
        }
    } else if (compilationMode == VPU::CompilationMode::DefaultHW) {
        const auto options = DefaultHWOptions30XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;

        // floatInputPrecision:
        // In case user passes -ip fp16/fp32 and enables FORCE_HOST_QUANTIZATION feature
        // we perform quantization on host.
        options->forceHostInputQuantization = config.get<FORCE_HOST_QUANTIZATION>() && prcInfo.floatInputPrecision;
        options->forceHostPrecisionLayoutConversion = config.get<FORCE_HOST_PRECISION_LAYOUT_CONVERSION>();

#if defined(_WIN32)
        if (prcInfo.inAndOutFp16) {
            options->forceHostPrecisionLayoutConversion = false;
            options->enableUseUserPrecision = false;
            options->enableUseUserLayout = false;
        }
#endif

        if (config.get<PLATFORM>() == InferenceEngine::VPUXConfigParams::VPUXPlatform::EMULATOR) {
            buildEMUDefaultHWModePipeline(pm, *options, log.nest());
        } else {
            buildDefaultHWModePipeline(pm, *options, log.nest());
        }
    } else if (compilationMode == VPU::CompilationMode::ShaveCodeGen) {
        buildShaveCodeGenPipeline30XX(pm, log.nest());
    } else {
        VPUX_THROW("Unsupported compilation mode '{0}'", compilationMode);
    }
}

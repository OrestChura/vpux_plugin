//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes.hpp"

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/utils/shape_infer.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"
#include "vpux/compiler/utils/types.hpp"

#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/DialectConversion.h>

#include <numeric>

using namespace vpux;

namespace {

//
// NormalizeL2Fusion
//

class NormalizeL2Fusion final : public mlir::OpRewritePattern<IE::ReduceL2Op> {
public:
    NormalizeL2Fusion(mlir::MLIRContext* ctx, Logger log): mlir::OpRewritePattern<IE::ReduceL2Op>(ctx), _log(log) {
        setDebugName("NormalizeL2Fusion");
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::ReduceL2Op origOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};
/*
    This pass convert this subgraph
     previousOp
        |
      /   \
     |     |
     |  ReduceL2
     |     |
     |   Clamp
      \   /
      Divide
       |
    to a single normalizeL2Op

*/
mlir::LogicalResult NormalizeL2Fusion::matchAndRewrite(IE::ReduceL2Op origOp, mlir::PatternRewriter& rewriter) const {
    _log.trace("Got reduceL2 '{0}' at '{1}'", origOp->getName(), origOp->getLoc());

    auto clampOp = mlir::dyn_cast<IE::ClampOp>(*(origOp.output().user_begin()));
    if (!clampOp) {
        _log.trace("Pattern not match");
        return mlir::failure();
    }
    auto divideOp = mlir::dyn_cast<IE::DivideOp>(*(clampOp.output().user_begin()));
    if (!divideOp) {
        _log.trace("Pattern not match");
        return mlir::failure();
    }
    // For NormalizeL2Op, it implement F(x) = x/Clamp(ReduceL2(x)), and DivedeOp implement input1/input2, so DivideOp's
    // input1 must be ReduceL2's input.
    if (divideOp.input1() != origOp.input()) {
        _log.trace("Pattern not match");
        return mlir::failure();
    }
    auto normalizeL2Op =
            rewriter.replaceOpWithNewOp<IE::NormalizeL2Op>(divideOp, origOp.input(), origOp.axes(), clampOp.minAttr(),
                                                           IE::EpsModeAttr::get(origOp.getContext(), IE::EpsMode::ADD));
    _log.trace("Replace '{0}' with new op '{1}'", origOp, normalizeL2Op);
    return mlir::success();
}
//
// NormalizeL2FusionPass
//

class NormalizeL2FusionPass final : public IE::NormalizeL2FusionBase<NormalizeL2FusionPass> {
public:
    explicit NormalizeL2FusionPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

//
// safeRunOnFunc
//

void NormalizeL2FusionPass::safeRunOnFunc() {
    auto& ctx = getContext();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<NormalizeL2Fusion>(&ctx, _log);

    auto func = getOperation();
    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createNormalizeL2FusionPass
//

std::unique_ptr<mlir::Pass> vpux::IE::createNormalizeL2FusionPass(Logger log) {
    return std::make_unique<NormalizeL2FusionPass>(log);
}

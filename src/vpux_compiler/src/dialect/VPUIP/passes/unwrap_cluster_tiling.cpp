//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPUIP/ops.hpp"
#include "vpux/compiler/dialect/VPUIP/passes.hpp"

#include <mlir/IR/BlockAndValueMapping.h>

using namespace vpux;

namespace {

//
// UnwrapClusterTilingPass
//

class UnwrapClusterTilingPass final : public VPUIP::UnwrapClusterTilingBase<UnwrapClusterTilingPass> {
public:
    explicit UnwrapClusterTilingPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

void UnwrapClusterTilingPass::safeRunOnFunc() {
    auto func = getOperation();
    mlir::OpBuilder builder(&func.getBody().front().front());

    func->walk([&](VPUIP::NCEClusterTilingOp clusterOp) {
        auto innerOp = clusterOp.getInnerTaskOp();
        _log.trace("Unwrap NCEClusterTilingOp with '{0}' - '{1}'", innerOp->getName(), clusterOp->getLoc());

        mlir::BlockAndValueMapping mapper;
        builder.setInsertionPointAfter(clusterOp);

        for (auto operand : innerOp->getOperands()) {
            if (auto blockArg = operand.dyn_cast<mlir::BlockArgument>()) {
                mapper.map(operand, clusterOp.getOperand(blockArg.getArgNumber()));
            }
        }
        auto newOp = builder.clone(*innerOp, mapper);

        VPUX_THROW_UNLESS(newOp->getNumResults() == clusterOp->getNumResults(),
                          "Mismatching number fo results: '{0}' != '{1}'", newOp->getNumResults(),
                          clusterOp->getNumResults());

        for (const auto& p : clusterOp->getResultTypes() | indexed) {
            newOp->getResult(p.index()).setType(p.value());
        }

        clusterOp->replaceAllUsesWith(newOp);
        innerOp->erase();
        clusterOp->erase();
    });
}

}  // namespace

//
// createUnwrapClusterTilingPass
//

std::unique_ptr<mlir::Pass> vpux::VPUIP::createUnwrapClusterTilingPass(Logger log) {
    return std::make_unique<UnwrapClusterTilingPass>(log);
}

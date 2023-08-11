//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/utils/broadcast_utils.hpp"

namespace vpux {
namespace IE {

SmallVector<int64_t> getBroadcastAxesNumpyBidirectional(ArrayRef<int64_t> inputShape, ArrayRef<int64_t> outputShape) {
    SmallVector<int64_t> broadcastAxes;
    const auto startAxis = static_cast<int64_t>(outputShape.size()) - static_cast<int64_t>(inputShape.size());
    VPUX_THROW_UNLESS(startAxis >= 0, "Broadcast axes not known deterministically");
    for (int64_t i = 0; i < static_cast<int64_t>(outputShape.size()); i++) {
        if (i < startAxis || outputShape[i] != inputShape[i - startAxis]) {
            broadcastAxes.push_back(i);
        }
    }
    return broadcastAxes;
}

SmallVector<int64_t> getBroadcastAxesExplicit(ArrayRef<int64_t> axesMapping, ArrayRef<int64_t> outputShape) {
    SmallVector<int64_t> broadcastAxes(outputShape.size());
    std::iota(broadcastAxes.begin(), broadcastAxes.end(), 0);
    for (auto i = axesMapping.rbegin(); i != axesMapping.rend(); ++i) {
        broadcastAxes.erase(broadcastAxes.begin() + *i);
    }
    return broadcastAxes;
}

}  // namespace IE
}  // namespace vpux

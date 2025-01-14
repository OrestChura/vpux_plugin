//
// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#ifndef SHARED_MODULES_NOP_H_
#define SHARED_MODULES_NOP_H_

#include "Op.h"

class NoOp: public Op
{
public:
    NoOp() = default;
    NoOp(t_MvTensorOpType /*op_type*/) : Op(kNone0) {}
};

#endif /* SHARED_MODULES_NOP_H_ */

//
// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//
#ifndef MPRINTF_H
#define MPRINTF_H

#ifndef __MOVICOMPILE__

#define MPRINTF(...) printf(__VA_ARGS__)

#else

#include <svuCommonShave.h>

#define MPRINTF(...)                \
    do {                            \
    scMutexRequest(MVTENSOR_MUTEX); \
    printf(__VA_ARGS__);            \
    scMutexRelease(MVTENSOR_MUTEX); \
    } while (false)

#endif

#endif

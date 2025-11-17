/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <config.h>
#include <mode/smp/smp.h>
#include <model/smp.h>

#ifdef ENABLE_SMP_SUPPORT
_Static_assert(CONFIG_MAX_NUM_NODES <= 64, "too many nodes");

/* Bitmap indicating whether a cpu is online or offline. */
uint64_t cpu_status = 1; // Core 0 starts as online

static inline cpu_id_t cpuIndexToID(word_t index)
{
    return BIT(index);
}

static inline void setCPUOnline(word_t index)
{
    cpu_status |= cpuIndexToID(index);
}

static inline void setCPUOffline(word_t index)
{
    cpu_status &= ~cpuIndexToID(index);
}

static inline bool_t isCPUOnline(word_t index)
{
    return (cpu_status & cpuIndexToID(index)) != 0;
}

#endif /* ENABLE_SMP_SUPPORT */


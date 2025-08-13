#pragma once

#include <config.h>

#ifdef CONFIG_PROFILER_ENABLE
void arm_handlePMUEvent(void);
void arm_initProfiler(void);

#define PMINTENSET "PMINTENSET_EL1"

#endif /* CONFIG_PROFILER_ENABLE */
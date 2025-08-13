#include <arch/profiling.h>
#include <api/faults.h>
#include <arch/arm/arch/64/mode/kernel/vspace.h>

#ifdef CONFIG_PROFILER_ENABLE
void arm_handlePMUEvent(void) {
    assert(isSchedulable(NODE_STATE(ksCurThread)));
    uint64_t pc = getRegister(NODE_STATE(ksCurThread), FaultIP);
    // Read the x29 register for the address of the current frame pointer
    word_t fp = getRegister(NODE_STATE(ksCurThread), X29);
    current_fault = seL4_Fault_PMUEvent_new(pc, fp);
    handleFault(NODE_STATE(ksCurThread));
}

void arm_initProfiler(void) {
    uint32_t val;
    MRS(PMINTENSET, val);
    // Setting the lower 32 bits all to 1
    // enables interrupts for all counters
    val |= UINT32_MAX;
    MSR(PMINTENSET, val);
}

#endif

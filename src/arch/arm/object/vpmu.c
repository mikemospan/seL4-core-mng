#include <config.h>
#include <arch/object/vpmu.h>
#include <mode/machine/registerset.h>

#ifdef CONFIG_THREAD_LOCAL_PMU

UP_STATE_DEFINE(pmu_state_t, cpu_pmu_state);
UP_STATE_DEFINE(pmu_state_t *, armCurVPMU);

/* FEAT_PMUv3_EXT */
static exception_t decodeVPMUControl_ReadEventCounter(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word counter = getSyscallArg(0, buffer);

    // Validate the counter is within range. We will match the number of counters available to the VPMU with that of hardware.
    uint32_t ctrl_reg;
    MRS(PMCR_EL0, ctrl_reg);
    uint32_t num_counters = (ctrl_reg >> 11) & 0x1f;

    if (counter > num_counters && counter < 32) {
        userError("PMUControl_CounterControl: Invalid counter.");
        current_syscall_error.type = seL4_InvalidArgument;
        return EXCEPTION_SYSCALL_ERROR;
    }
    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        // Current VPMU is running on hardware, so read directly from hardware.
        uint32_t cnt_sel = 1 << counter;

        uint32_t counter_value;

        MSR(PMSELR_EL0, cnt_sel);
        MRS(PMXEVCNTR_EL0, counter_value);
        setRegister(NODE_STATE(ksCurThread), msgRegisters[0], counter_value);
    } else {
        setRegister(NODE_STATE(ksCurThread), msgRegisters[0], vpmu->event_counters[counter]);
    }

    return EXCEPTION_NONE;
}

/* FEAT_PMUv3_EXT */
static exception_t decodeVPMUControl_WriteEventCounter(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word counter = getSyscallArg(0, buffer);
    seL4_Word value = getSyscallArg(1, buffer);
    seL4_Word event = getSyscallArg(2, buffer);

    uint32_t ctrl_reg;
    MRS(PMCR_EL0, ctrl_reg);
    uint32_t num_counters = (ctrl_reg >> 11) & 0x1f;

    if (counter > num_counters && counter < 32) {
        userError("PMUControl_CounterControl: Invalid counter.");
        current_syscall_error.type = seL4_InvalidArgument;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        uint32_t cnt_sel = 1 << counter;

        MSR(PMSELR_EL0, cnt_sel);
        MSR(PMXEVCNTR_EL0, value);
        MSR(PMXEVTYPER_EL0, event);
    } else {
        vpmu->event_counters[counter] = value;
        vpmu->event_type[counter] = event;
    }
    return EXCEPTION_NONE;
}

static exception_t decodeVPMUControl_ReadCycleCounter(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word cycle_counter;

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MRS(PMU_CYCLE_CTR, cycle_counter);
    } else {
        cycle_counter = vpmu->cycle_counter;
    }

    setRegister(NODE_STATE(ksCurThread), msgRegisters[0], cycle_counter);

    return EXCEPTION_NONE;
}

static exception_t decodeVPMUControl_WriteCycleCounter(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word counter_value = getSyscallArg(0, buffer);

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MSR(PMU_CYCLE_CTR, counter_value);
    } else {
        vpmu->cycle_counter = counter_value;
    }

    return EXCEPTION_NONE;
}


/* FEAT_PMUv3_EXT */
static exception_t decodeVPMUControl_CounterControl(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word cntl_val = getSyscallArg(0, buffer);

    if (cntl_val > 2) {
        userError("PMUControl_CounterControl: Invalid control value. Must be 0, 1 or 2.");
        current_syscall_error.type = seL4_InvalidArgument;
        return EXCEPTION_SYSCALL_ERROR;
    }

    uint32_t pmcr = 0;
    uint32_t pmcntenset = 0;
    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MRS(PMCR_EL0, pmcr);
        MRS(PMCNTENSET_EL0, pmcntenset);
    } else {
        pmcr = vpmu->pmcr;
        pmcntenset = vpmu->pmcntenset;
    }

    switch(cntl_val) {
        case 0: {
            uint32_t mask = 0;

            /* Disable Performance Counter */
            mask = 0;
            mask |= (1 << 0);
            pmcr = (pmcr & ~mask);

            /* Disable cycle counter register */
            mask = 0;
            mask |= (1 << 31);
            pmcntenset = (pmcntenset & ~mask);
            
            break;
        }
        case 1: {
            pmcr |= BIT(0);
            pmcntenset = BIT(31);
            break;
        }
        case 2: {
            uint32_t mask = 0;
            mask |= (1 << 1); /* Cycle counter reset */
            mask |= (1 << 2); /* Reset all counters */
            pmcr = (pmcr & ~mask);
            break;
        }
        default:
            break;
    }

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MSR(PMCR_EL0, pmcr);
        MSR(PMCNTENSET_EL0, pmcntenset);
    } else {
        vpmu->pmcr = pmcr;
        vpmu->pmcntenset = pmcntenset;
    }

    return EXCEPTION_NONE;
}

/* FEAT_PMUv3_EXT */
static exception_t decodeVPMUControl_ReadInterruptValue(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    // Get the interrupt flag from the PMU
    uint32_t irqFlag = 0;

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MRS(PMOVSCLR_EL0, irqFlag);
    } else {
        irqFlag = vpmu->pmovsclr;
    }

    setRegister(NODE_STATE(ksCurThread), msgRegisters[0], irqFlag);

    return EXCEPTION_NONE;
}

static exception_t decodeVPMUControl_WriteInterruptValue(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word interrupt_value = getSyscallArg(0, buffer);

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MSR(PMOVSCLR_EL0, interrupt_value);
    } else {
        vpmu->pmovsclr = interrupt_value;
    }

    return EXCEPTION_NONE;
}

static exception_t decodeVPMUControl_InterruptControl(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    seL4_Word interrupt_ctl = getSyscallArg(0, buffer);

    if (ARCH_NODE_STATE(armCurVPMU) == vpmu) {
        MSR(PMINTENSET_EL1, interrupt_ctl);
    } else {
        vpmu->pmintenset = interrupt_ctl;
    }
    return EXCEPTION_NONE;
}

static exception_t decodeVPMUControl_NumCounters(word_t length, cap_t cap, word_t *buffer, pmu_state_t *vpmu)
{
    // Find number of counters available on hardware, the VPMU will match this
    uint32_t ctrl_reg;
    MRS(PMCR_EL0, ctrl_reg);
    uint32_t num_counters = (ctrl_reg >> 11) & 0x1f;

    setRegister(NODE_STATE(ksCurThread), msgRegisters[0], num_counters);

    return EXCEPTION_NONE;
}

exception_t decodeARMVPMUInvocation(word_t label, unsigned int length, cptr_t cptr,
                                         cte_t *srcSlot, cap_t cap,
                                         bool_t call, word_t *buffer)
{
    pmu_state_t *vpmu = VPMU_PTR(cap_vpmu_cap_get_capPMUPtr(cap));

    switch(label) {
        case VPMUReadEventCounter:
            return decodeVPMUControl_ReadEventCounter(length, cap, buffer, vpmu);
        case VPMUWriteEventCounter:
            return decodeVPMUControl_WriteEventCounter(length, cap, buffer, vpmu);
        case VPMUReadCycleCounter:
            return decodeVPMUControl_ReadCycleCounter(length, cap, buffer, vpmu);
        case VPMUWriteCycleCounter:
            return decodeVPMUControl_WriteCycleCounter(length, cap, buffer, vpmu);
        case VPMUCounterControl:
            return decodeVPMUControl_CounterControl(length, cap, buffer, vpmu);
        case VPMUReadInterruptValue:
            return decodeVPMUControl_ReadInterruptValue(length, cap, buffer, vpmu);
        case VPMUWriteInterruptValue:
            return decodeVPMUControl_WriteInterruptValue(length, cap, buffer, vpmu);
        case VPMUInterruptControl:
            return decodeVPMUControl_InterruptControl(length, cap, buffer, vpmu);
        case VPMUNumCounters:
            return decodeVPMUControl_NumCounters(length, cap, buffer, vpmu);
        default:
            userError("PMUControl invocation: Illegal operation attempted.");
            current_syscall_error.type = seL4_IllegalOperation;
            return EXCEPTION_SYSCALL_ERROR;
    }

    return EXCEPTION_NONE;
}
#endif /* CONFIG_THREAD_LOCAL_PMU */

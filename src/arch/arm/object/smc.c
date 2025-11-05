/*
 * Copyright 2021, DornerWorks Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <config.h>

#ifdef CONFIG_ALLOW_SMC_CALLS
#include <arch/object/smc.h>

#define PSCI_CPU_OFF            0x84000002
#define PSCI_CPU_SUSPEND        0xC4000001
#define PSCI_POWER_STATE_MASK   (1u << 16)

compile_assert(n_msgRegisters_less_than_smc_regs, n_msgRegisters <= NUM_SMC_REGS);

static exception_t invokeSMCCall(word_t *buffer, bool_t call)
{
    word_t i;
    seL4_Word arg[NUM_SMC_REGS];
    word_t *ipcBuffer;

    for (i = 0; i < NUM_SMC_REGS; i++) {
        arg[i] = getSyscallArg(i, buffer);
    }

    ipcBuffer = lookupIPCBuffer(true, NODE_STATE(ksCurThread));

    seL4_Word a0 = arg[0];
    seL4_Word a1 = arg[1];
    seL4_Word a2 = arg[2];
    seL4_Word a3 = arg[3];
    seL4_Word a4 = arg[4];
    seL4_Word a5 = arg[5];
    seL4_Word a6 = arg[6];
    seL4_Word a7 = arg[7];

    if (a0 == PSCI_CPU_OFF || a0 == PSCI_CPU_SUSPEND) {
        /*
         * Disable timer interrupts to avoid the TF-A from rejecting our request due to
         * an in-flight interrupt. We also unlock the big kernel lock to allow other
         * cores to make progress since we don't expect to return.
         */
        setIRQState(IRQInactive, CORE_IRQ_TO_IRQT(getCurrentCPUIndex(), KERNEL_TIMER_IRQ));
        NODE_UNLOCK_IF_HELD;
    }

    /* Force the values into x0..x7 *immediately before* the SMC */
    register seL4_Word r0 asm("x0") = a0;
    register seL4_Word r1 asm("x1") = a1;
    register seL4_Word r2 asm("x2") = a2;
    register seL4_Word r3 asm("x3") = a3;
    register seL4_Word r4 asm("x4") = a4;
    register seL4_Word r5 asm("x5") = a5;
    register seL4_Word r6 asm("x6") = a6;
    register seL4_Word r7 asm("x7") = a7;

    asm volatile("smc #0\n"
                : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6), "+r"(r7)
                :: "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");

    bool_t was_cpu_standby = a0 == PSCI_CPU_SUSPEND && !(a1 & PSCI_POWER_STATE_MASK);
    if (was_cpu_standby) {
        /* Re-aquire the big kernel lock, and also re-enable the timer interrupt. */
        NODE_LOCK_SYS;
        setIRQState(IRQTimer, CORE_IRQ_TO_IRQT(getCurrentCPUIndex(), KERNEL_TIMER_IRQ));
    }

    arg[0] = r0;
    arg[1] = r1;
    arg[2] = r2;
    arg[3] = r3;
    arg[4] = r4;
    arg[5] = r5;
    arg[6] = r6;
    arg[7] = r7;

    if (call) {
        for (i = 0; i < n_msgRegisters; i++) {
            setRegister(NODE_STATE(ksCurThread), msgRegisters[i], arg[i]);
        }

        if (ipcBuffer != NULL) {
            for (; i < NUM_SMC_REGS; i++) {
                ipcBuffer[i + 1] = arg[i];
            }
        }

        setRegister(NODE_STATE(ksCurThread), badgeRegister, 0);
        setRegister(NODE_STATE(ksCurThread), msgInfoRegister, wordFromMessageInfo(
                        seL4_MessageInfo_new(0, 0, 0, i)));
    }
    setThreadState(NODE_STATE(ksCurThread), ThreadState_Running);
    return EXCEPTION_NONE;
}

exception_t decodeARMSMCInvocation(word_t label, word_t length, cptr_t cptr,
                                   cte_t *srcSlot, cap_t cap, bool_t call, word_t *buffer)
{
    if (label != ARMSMCCall) {
        userError("ARMSMCInvocation: Illegal operation.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (length < NUM_SMC_REGS) {
        userError("ARMSMCCall: Truncated message.");
        current_syscall_error.type = seL4_TruncatedMessage;
        return EXCEPTION_SYSCALL_ERROR;
    }

    word_t badge = cap_smc_cap_get_capSMCBadge(cap);
    word_t smc_func_id = getSyscallArg(0, buffer);

    if (badge != 0 && badge != smc_func_id) {
        userError("ARMSMCCall: Illegal operation.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
    return invokeSMCCall(buffer, call);
}

#endif

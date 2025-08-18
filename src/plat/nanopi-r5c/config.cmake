#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
# Copyright 2022, Capgemini Engineering
#
# SPDX-License-Identifier: GPL-2.0-only
#

declare_platform(nanopi-r5c KernelPlatformNanopiR5c PLAT_NANOPI_R5C KernelArchARM)

if(KernelPlatformNanopiR5c)
    declare_seL4_arch(aarch64)

    set(KernelArmCortexA55 ON)
    set(KernelArchArmV8a ON)
    set(KernelArmGicV3 ON)
    config_set(KernelARMPlatform ARM_PLAT ${KernelPlatform})
    list(APPEND KernelDTSList "tools/dts/${KernelPlatform}.dts")
    list(APPEND KernelDTSList "src/plat/${KernelPlatform}/overlay-${KernelPlatform}.dts")
    declare_default_headers(
        TIMER_FREQUENCY 8000000 # idk
        MAX_IRQ 282
        TIMER drivers/timer/arm_generic.h
        INTERRUPT_CONTROLLER arch/machine/gic_v3.h
        NUM_PPI 32
	# TODO: I do not know how to come up with this number. This was copy and
	# pasted from another config.cmake
        KERNEL_WCET 10u
    )
endif()

add_sources(
    DEP "KernelPlatformNanopiR5c"
    CFILES src/arch/arm/machine/gic_v3.c src/arch/arm/machine/l2c_nop.c
)

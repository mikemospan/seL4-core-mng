/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <config.h>
#include <util.h>
#include <arch/machine/hardware.h>
#include <sel4/plat/api/constants.h>

/* TODO: Update this diagram. */
/* EL2 kernel address map:
 *
 * The EL2 mode kernel uses TTBR0_EL2 which covers the range of
 * 0x0 - 0x0000ffffffffffff (48 bits of vaddrspace).
 *
 * The CPU on the TX1 only allows for 48-bits of addressable virtual memory.
 * Within the seL4 EL2 kernel's separate vaddrspace, it uses
 * the 512 GiB at the top of that 48 bits of addressable
 * virtual memory.
 *
 * In EL2 there is no canonical-high portion of the address space since
 * address tagging is not supported in EL2. Therefore the kernel is linked
 * to the canonical lower portion of the address space (all the unused top bits
 * are set to 0, not 1).
 *
 * The memory map used by the EL2 kernel's separate address space
 * ends up looking something like this:
 *
 * +-----------------------------------+ <- 0xFFFFFFFF_FFFFFFFF
 * | Canonical high portion - unusable |
 * | virtual addrs                     |
 * +-----------------------------------+ <- PPTR_TOP: 256TiB mark (top of 48 bits)
 * | seL4 EL2 kernel                   |    ^
 * |                                   |    |
 * |                                   |    512GiB
 * |                                   |    of EL2 kernel windowing
 * |                                   |    into memory.
 * |                                   |    |
 * |                                   |    v
 * +-----------------------------------+ <- PPTR_BASE: 256TiB minus 512GiB.
 * | Unused virtual addresses within   |    ^
 * | the EL2 kernel's                  |    |
 * | separate vaddrspace.              |    Rest of the
 * |                                   |    EL2 kernel
 * |                                   |    vaddrspace, unused,
 * |                                   |    which is the rest of
 * |                                   |    the lower 256 TiB.
 * |                                   |    |
 * |                                   |    v
 * +-----------------------------------+ <- 0x0
 *
 * !defined(CONFIG_ARM_HYPERVISOR_SUPPORT)
 *
 *          2^64 +-------------------+
 *               | Kernel Page PDPT  | --+
 *   2^64 - 2^39 +-------------------+ PPTR_BASE
 *               |    TLB Bitmaps    |   |                  // FIXME: TLB Bitmaps are an x86 thing?
 *               +-------------------+   |
 *               |                   |   |
 *               |     Unmapped      |   |
 *               |                   |   |
 *   2^64 - 2^48 +-------------------+   |
 *               |                   |   |
 *               |   Unaddressable   |   |
 *               |                   |   |
 *          2^48 +-------------------+ USER_TOP
 *               |                   |   |
 *               |       User        |   |
 *               |                   |   |
 *           0x0 +-------------------+   |
 *                                       |
 *                         +-------------+
 *                         |
 *                         v
 *          2^64 +-------------------+
 *               |                   |
 *               |                   |     +------+
 *               | Kernel Page Table | --> |  PD  | ----+
 *               |                   |     +------+     |
 *               |                   |                  |
 *   2^64 - 2^30 +-------------------+ PPTR_TOP         |
 *               |                   |                  |
 *               |  Physical Memory  |                  |
 *               |       Window      |                  |
 *               |                   |                  |
 *               +-------------------+                  |
 *               |                   |                  |
 *               |                   |     +------+     |
 *               |    Kernel ELF     | --> |  PD  |     |
 *               |                   |     +------+     |
 *               |                   |                  |
 *               +-------------------+ KERNEL_ELF_BASE  |
 *               |                   |                  |
 *               |  Physical Memory  |                  |
 *               |       Window      |                  |
 *               |                   |                  |
 *   2^64 - 2^39 +-------------------+ PPTR_BASE        |
 *                                                      |
 *                                +---------------------+
 *                                |
 *                                v
 *   2^64 - 2^21 + 2^12 +-------------------+
 *                      |                   |
 *                      |  Kernel Devices   |
 *                      |                   |
 *          2^64 - 2^21 +-------------------+ KDEV_BASE
 *
 *
 * defined(CONFIG_ARM_HYPERVISOR_SUPPORT)
 *
 *          2^64 +-------------------+
 *               |                   |
 *               |   Unaddressable   |
 *               |                   |
 *          2^48 +-------------------+
 *               | Kernel Page PDPT  | --+
 *   2^48 - 2^39 +-------------------+ PPTR_BASE
 *               |    TLB Bitmaps    |   |
 *               +-------------------+   |
 *               |                   |   |
 *               |     Unmapped      |   |
 *               |                   |   |
 *           0x0 +-------------------+   |
 *                                       |
 *                         +-------------+
 *                         |
 *                         v
 *          2^48 +-------------------+
 *               |                   |
 *               |                   |     +------+
 *               | Kernel Page Table | --> |  PD  | ----+
 *               |                   |     +------+     |
 *               |                   |                  |
 *   2^48 - 2^30 +-------------------+ PPTR_TOP         |
 *               |                   |                  |
 *               |  Physical Memory  |                  |
 *               |       Window      |                  |
 *               |                   |                  |
 *               +-------------------+                  |
 *               |                   |                  |
 *               |                   |     +------+     |
 *               |    Kernel ELF     | --> |  PD  |     |
 *               |                   |     +------+     |
 *               |                   |                  |
 *               +-------------------+ KERNEL_ELF_BASE  |
 *               |                   |                  |
 *               |  Physical Memory  |                  |
 *               |       Window      |                  |
 *               |                   |                  |
 *   2^48 - 2^39 +-------------------+ PPTR_BASE        |
 *                                                      |
 *                                +---------------------+
 *                                |
 *                                v
 *   2^48 - 2^21 + 2^12 +-------------------+
 *                      |                   |
 *                      |  Kernel Devices   |
 *                      |                   |
 *          2^48 - 2^21 +-------------------+ KDEV_BASE
 *
 */

/* last accessible virtual address in user space */
#define USER_TOP seL4_UserTop

/* The first physical address to map into the kernel's physical memory
 * window */
#define PADDR_BASE UL_CONST(0x0)

/* The base address in virtual memory to use for the 1:1 physical memory
 * mapping */
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define PPTR_BASE UL_CONST(0x0000008000000000)  /* 2^48 - 2^39 */
#else
#define PPTR_BASE UL_CONST(0xffffff8000000000)  /* 2^464 - 2^39 */
#endif

/* Top of the physical memory window; exclusive. */
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define PPTR_TOP UL_CONST(0x000000ffc0000000)   /* 2^48 - 2^30 */
#else
#define PPTR_TOP UL_CONST(0xffffffffc0000000)   /* 2^64 - 2^30 */
#endif

/* The physical memory address to use for mapping the kernel ELF */
#define KERNEL_ELF_PADDR_BASE physBase()
/* For use by the linker (only integer constants allowed) */
#define KERNEL_ELF_PADDR_BASE_RAW (PHYS_BASE_RAW)

/* The KERNEL_ELF_BASE virtual address is placed at an arbitrary high memory
 * address above PPTR_TOP. It is mapped using 2MiB pages.
 * Arbitrarily, it is placed directly above PPTR_TOP.
 */
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define KERNEL_ELF_BASE UL_CONST(0x000000ffc0000000)  /* 2^48 - 2^30 */
#else
#define KERNEL_ELF_BASE UL_CONST(0xffffffffc0000000)  /* 2^64 - 2^30 */
#endif

/* For use by the linker (as it is shared with arm32) */
#define KERNEL_ELF_BASE_RAW KERNEL_ELF_BASE

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
/* The base address in virtual memory to use for kernel devices, which are
 * mapped using a PT containing 4K pages.  */
#define KDEV_BASE UL_CONST(0x000000ffffe00000)  /* 2^48 - 2^21 */
#else
#define KDEV_BASE UL_CONST(0xffffffffffe00000)  /* 2^64 - 2^21 */
#endif

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
/* The log buffer is placed before the device region using a 2MiB page. */
#define KS_LOG_BASE UL_CONST(0x000000ffffc00000)  /* 2^48 - 2 * 2^21 */
#else
#define KS_LOG_BASE UL_CONST(0xffffffffffc00000)  /* 2^64 - 2 * 2^21 */
#endif

#ifndef __ASSEMBLER__
/* All PPTR addresses must be canonical to be able to be stored in caps or objects.
   Check that all UTs that are created will have valid address in the PPTR space.
   For non-hyp, PPTR_BASE is in the top part of the address space and device untyped
   addresses are allowed to be large enough to overflow and be in the bottom half of
   the address space.  However, when the kernel is in EL2 it is not possible to safely
   overflow without going into address ranges that are non-canonical.  These static
   asserts check that the kernel config won't lead to UTs being created that aren't
   representable. */
compile_assert(ut_max_less_than_canonical, CONFIG_PADDR_USER_DEVICE_TOP <= BIT(47));
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
compile_assert(ut_max_is_canonical, (PPTR_BASE + CONFIG_PADDR_USER_DEVICE_TOP) <= BIT(48));
#endif
#endif

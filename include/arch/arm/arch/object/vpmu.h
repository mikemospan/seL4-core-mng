#pragma once

#include <types.h>
#include <api/failures.h>
#include <object/structures.h>

#ifdef CONFIG_THREAD_LOCAL_PMU

exception_t decodeARMVPMUInvocation(word_t label, unsigned int length, cptr_t cptr,
                                         cte_t *srcSlot, cap_t cap,
                                         bool_t call, word_t *buffer);
#endif /* CONFIG_THREAD_LOCAL_PMU */

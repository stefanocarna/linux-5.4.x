/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_DYNAMIC_PATCHES_H
#define _ASM_X86_DYNAMIC_PATCHES_H

#include <asm/percpu.h>

#ifndef __ASSEMBLY__

DECLARE_PER_CPU(u32, cpu_dynamic_patches);

#define check_cpu_dynamic_flag(f) (!!(*this_cpu_ptr(&(cpu_dynamic_patches)) & f))

#endif /* __ASSEMBLY__ */


/* Dynamic CPU Patch flags */
#define DCP_PTI				0
#define DCP_FENCE_SWAP_KERNEL		1
#define DCP_RETPOLINE			2
#define DCP_RSB_CTXSW			3
#define DCP_USE_IBPB			4

#define DCP_COND_STIBP			5
#define DCP_COND_IBPB			6
#define DCP_ALWAYS_IBPB			7


#define DCP_RETPOLINE_SHIFT		_BITUL(DCP_RETPOLINE)
#define DCP_RSB_CTXSW_SHIFT		_BITUL(DCP_RSB_CTXSW)
#define DCP_USE_IBPB_SHIFT		_BITUL(DCP_USE_IBPB)

#define DCP_COND_STIBP_SHIFT		_BITUL(DCP_COND_STIBP)
#define DCP_COND_IBPB_SHIFT		_BITUL(DCP_COND_IBPB)
#define DCP_ALWAYS_IBPB_SHIFT		_BITUL(DCP_ALWAYS_IBPB)


#define skip_switch_to_cond_stibp	(!check_cpu_dynamic_flag(DCP_COND_STIBP_SHIFT))
#define skip_switch_mm_cond_ibpb	(!check_cpu_dynamic_flag(DCP_COND_IBPB_SHIFT))
#define skip_switch_mm_always_ibpb	(!check_cpu_dynamic_flag(DCP_ALWAYS_IBPB_SHIFT))

/* ASM macro */
#define dynamic_patch_save(bit, reg, skip_lab)		\
	pushq	reg;					\
	movq 	PER_CPU_VAR(cpu_dynamic_patches), reg;	\
	bt 	$##bit, reg;				\
	jnc 	skip_lab

#define dynamic_patch_restore(reg, skip_lab)		\
skip_lab:						\
	popq reg

#endif /* _ASM_X86_DYNAMIC_PATCHES_H */

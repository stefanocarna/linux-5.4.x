/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_DYNAMIC_PTI_H
#define _ASM_X86_DYNAMIC_PTI_H
#ifndef __ASSEMBLY__

#ifdef CONFIG_PAGE_TABLE_ISOLATION
extern void dynamic_pti_suspect_thread(pid_t pid);
extern void dynamic_pti_trust_thread(pid_t pid);
#endif

#endif /* __ASSEMBLY__ */
#endif /* _ASM_X86_DYNAMIC_PTI_H */

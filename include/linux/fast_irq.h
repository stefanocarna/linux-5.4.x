/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FAST_IRQ_H
#define _FAST_IRQ_H

typedef int fast_handler_t(void);

extern unsigned long fast_vectors[];

extern int __must_check
request_fast_irq(unsigned int fast_irq, fast_handler_t handler);

extern int __must_check
free_fast_irq(unsigned int fast_irq);

#endif /* _FAST_IRQ_H */
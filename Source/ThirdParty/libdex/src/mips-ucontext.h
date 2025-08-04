typedef struct mcontext mcontext_t;
typedef struct ucontext ucontext_t;

/*
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)ucontext.h  8.1 (Berkeley) 6/10/93
 *      JNPR: ucontext.h,v 1.2 2007/08/09 11:23:32 katta
 * $FreeBSD: src/sys/mips/include/ucontext.h,v 1.2 2010/01/10 19:50:24 imp Exp $
 */

struct  mcontext {
  /*
   * These fields must match the corresponding fields in struct
   * sigcontext which follow 'sc_mask'. That way we can support
   * struct sigcontext and ucontext_t at the same time.
   */
  int     mc_onstack;     /* sigstack state to restore */
  int     mc_pc;          /* pc at time of signal */
  int     mc_regs[32];    /* processor regs 0 to 31 */
  int     sr;             /* status register */
  int     mullo, mulhi;   /* mullo and mulhi registers... */
  int     mc_fpused;      /* fp has been used */
  int     mc_fpregs[33];  /* fp regs 0 to 31 and csr */
  int     mc_fpc_eir;     /* fp exception instruction reg */
  void   *mc_tls;         /* pointer to TLS area */
  int     __spare__[8];   /* reserved */
};

struct ucontext {
  /*
   * Keep the order of the first two fields. Also,
   * keep them the first two fields in the structure.
   * This way we can have a union with struct
   * sigcontext and ucontext_t. This allows us to
   * support them both at the same time.
   * note: the union is not defined, though.
   */
  sigset_t           uc_sigmask;
  mcontext_t         uc_mcontext;

  struct __ucontext *uc_link;
  stack_t            uc_stack;
  int                uc_flags;
  int                __spare__[4];
};

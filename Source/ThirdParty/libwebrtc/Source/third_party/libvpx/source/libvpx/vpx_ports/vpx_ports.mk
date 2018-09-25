##
##  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


PORTS_SRCS-yes += vpx_ports.mk

PORTS_SRCS-yes += bitops.h
PORTS_SRCS-yes += mem.h
PORTS_SRCS-yes += msvc.h
PORTS_SRCS-yes += system_state.h
PORTS_SRCS-yes += vpx_timer.h

ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
PORTS_SRCS-yes += emms.asm
PORTS_SRCS-yes += x86.h
PORTS_SRCS-yes += x86_abi_support.asm
endif

PORTS_SRCS-$(ARCH_ARM) += arm_cpudetect.c
PORTS_SRCS-$(ARCH_ARM) += arm.h

PORTS_SRCS-$(ARCH_PPC) += ppc_cpudetect.c
PORTS_SRCS-$(ARCH_PPC) += ppc.h

ifeq ($(ARCH_MIPS), yes)
PORTS_SRCS-yes += asmdefs_mmi.h
endif

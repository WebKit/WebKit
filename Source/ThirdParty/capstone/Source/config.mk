# This file contains all customized compile options for Capstone.
# Consult COMPILE.TXT & docs/README for details.

################################################################################
# Specify which archs you want to compile in. By default, we build all archs.

CAPSTONE_ARCHS ?= arm aarch64 m68k mips powerpc sparc systemz x86 xcore tms320c64x m680x evm


################################################################################
# Comment out the line below ('CAPSTONE_USE_SYS_DYN_MEM = yes'), or change it to
# 'CAPSTONE_USE_SYS_DYN_MEM = no' if do NOT use malloc/calloc/realloc/free/
# vsnprintf() provided by system for internal dynamic memory management.
#
# NOTE: in that case, specify your own malloc/calloc/realloc/free/vsnprintf()
# functions in your program via API cs_option(), using CS_OPT_MEM option type.

CAPSTONE_USE_SYS_DYN_MEM ?= yes


################################################################################
# Change 'CAPSTONE_DIET = no' to 'CAPSTONE_DIET = yes' to make the library
# more compact: use less memory & smaller in binary size.
# This setup will remove the @mnemonic & @op_str data, plus semantic information
# such as @regs_read/write & @group. The amount of binary size reduced is
# up to 50% in some individual archs.
#
# NOTE: we still keep all those related fileds @mnemonic, @op_str, @regs_read,
# @regs_write, @groups, etc in fields in cs_insn structure regardless, but they
# will not be updated (i.e empty), thus become irrelevant.

CAPSTONE_DIET ?= no


################################################################################
# Change 'CAPSTONE_X86_REDUCE = no' to 'CAPSTONE_X86_REDUCE = yes' to remove
# non-critical instruction sets of X86, making the binary size smaller by ~60%.
# This is desired in special cases, such as OS kernel, where these kind of
# instructions are not used.
#
# The list of instruction sets to be removed includes:
# - Floating Point Unit (FPU)
# - MultiMedia eXtension (MMX)
# - Streaming SIMD Extensions (SSE)
# - 3DNow
# - Advanced Vector Extensions (AVX)
# - Fused Multiply Add Operations (FMA)
# - eXtended Operations (XOP)
# - Transactional Synchronization Extensions (TSX)
#
# Due to this removal, the related instructions are nolonger supported.
#
# By default, Capstone is compiled with 'CAPSTONE_X86_REDUCE = no',
# thus supports complete X86 instructions.

CAPSTONE_X86_REDUCE ?= no

################################################################################
# Change 'CAPSTONE_X86_ATT_DISABLE = no' to 'CAPSTONE_X86_ATT_DISABLE = yes' to
# disable AT&T syntax on x86 to reduce library size.

CAPSTONE_X86_ATT_DISABLE ?= no

################################################################################
# Change 'CAPSTONE_STATIC = yes' to 'CAPSTONE_STATIC = no' to avoid building
# a static library.

CAPSTONE_STATIC ?= yes


################################################################################
# Change 'CAPSTONE_SHARED = yes' to 'CAPSTONE_SHARED = no' to avoid building
# a shared library.

CAPSTONE_SHARED ?= yes

################################################################################
# Change 'CAPSTONE_HAS_OSXKERNEL = no' to 'CAPSTONE_HAS_OSXKERNEL = yes' to
# enable OS X kernel embedding support. If 'CAPSTONE_USE_SYS_DYN_MEM = yes',
# then kern_os_* functions are used for memory management.

CAPSTONE_HAS_OSXKERNEL ?= no

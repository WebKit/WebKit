# Capstone Disassembly Engine
# By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014

include config.mk
include pkgconfig.mk	# package version
include functions.mk

# Verbose output?
V ?= 0

ifeq ($(PKG_EXTRA),)
PKG_VERSION = $(PKG_MAJOR).$(PKG_MINOR)
else
PKG_VERSION = $(PKG_MAJOR).$(PKG_MINOR).$(PKG_EXTRA)
endif

ifeq ($(CROSS),)
CC ?= cc
AR ?= ar
RANLIB ?= ranlib
STRIP ?= strip
else
CC = $(CROSS)gcc
AR = $(CROSS)ar
RANLIB = $(CROSS)ranlib
STRIP = $(CROSS)strip
endif

ifneq (,$(findstring yes,$(CAPSTONE_DIET)))
CFLAGS ?= -Os
CFLAGS += -DCAPSTONE_DIET
else
CFLAGS ?= -O3
endif

ifneq (,$(findstring yes,$(CAPSTONE_X86_ATT_DISABLE)))
CFLAGS += -DCAPSTONE_X86_ATT_DISABLE
endif

CFLAGS += -fPIC -Wall -Iinclude

ifeq ($(CAPSTONE_USE_SYS_DYN_MEM),yes)
CFLAGS += -DCAPSTONE_USE_SYS_DYN_MEM
endif

ifeq ($(CAPSTONE_HAS_OSXKERNEL), yes)
CFLAGS += -DCAPSTONE_HAS_OSXKERNEL
SDKROOT ?= $(shell xcodebuild -version -sdk macosx Path)
CFLAGS += -mmacosx-version-min=10.5 \
		  -isysroot$(SDKROOT) \
		  -I$(SDKROOT)/System/Library/Frameworks/Kernel.framework/Headers \
		  -mkernel \
		  -fno-builtin
endif

PREFIX ?= /usr
DESTDIR ?=
ifndef BUILDDIR
BLDIR = .
OBJDIR = .
else
BLDIR = $(abspath $(BUILDDIR))
OBJDIR = $(BLDIR)/obj
endif
INCDIR ?= $(PREFIX)/include

UNAME_S := $(shell uname -s)

LIBDIRARCH ?= lib
# Uncomment the below line to installs x86_64 libs to lib64/ directory.
# Or better, pass 'LIBDIRARCH=lib64' to 'make install/uninstall' via 'make.sh'.
#LIBDIRARCH ?= lib64
LIBDIR ?= $(PREFIX)/$(LIBDIRARCH)
BINDIR = $(PREFIX)/bin

LIBDATADIR ?= $(LIBDIR)

# Don't redefine $LIBDATADIR when global environment variable
# USE_GENERIC_LIBDATADIR is set. This is used by the pkgsrc framework.

ifndef USE_GENERIC_LIBDATADIR
ifeq ($(UNAME_S), FreeBSD)
LIBDATADIR = $(PREFIX)/libdata
endif
ifeq ($(UNAME_S), DragonFly)
LIBDATADIR = $(PREFIX)/libdata
endif
endif

INSTALL_BIN ?= install
INSTALL_DATA ?= $(INSTALL_BIN) -m0644
INSTALL_LIB ?= $(INSTALL_BIN) -m0755

LIBNAME = capstone


DEP_ARM =
DEP_ARM += $(wildcard arch/ARM/ARM*.inc)

LIBOBJ_ARM =
ifneq (,$(findstring arm,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_ARM
	LIBSRC_ARM += $(wildcard arch/ARM/ARM*.c)
	LIBOBJ_ARM += $(LIBSRC_ARM:%.c=$(OBJDIR)/%.o)
endif

DEP_ARM64 =
DEP_ARM64 += $(wildcard arch/AArch64/AArch64*.inc)

LIBOBJ_ARM64 =
ifneq (,$(findstring aarch64,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_ARM64
	LIBSRC_ARM64 += $(wildcard arch/AArch64/AArch64*.c)
	LIBOBJ_ARM64 += $(LIBSRC_ARM64:%.c=$(OBJDIR)/%.o)
endif


DEP_M68K =
DEP_M68K += $(wildcard arch/M68K/M68K*.h)

LIBOBJ_M68K =
ifneq (,$(findstring m68k,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_M68K
	LIBSRC_M68K += $(wildcard arch/M68K/M68K*.c)
	LIBOBJ_M68K += $(LIBSRC_M68K:%.c=$(OBJDIR)/%.o)
endif

DEP_MIPS =
DEP_MIPS += $(wildcard arch/Mips/Mips*.inc)

LIBOBJ_MIPS =
ifneq (,$(findstring mips,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_MIPS
	LIBSRC_MIPS += $(wildcard arch/Mips/Mips*.c)
	LIBOBJ_MIPS += $(LIBSRC_MIPS:%.c=$(OBJDIR)/%.o)
endif


DEP_PPC =
DEP_PPC += $(wildcard arch/PowerPC/PPC*.inc)

LIBOBJ_PPC =
ifneq (,$(findstring powerpc,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_POWERPC
	LIBSRC_PPC += $(wildcard arch/PowerPC/PPC*.c)
	LIBOBJ_PPC += $(LIBSRC_PPC:%.c=$(OBJDIR)/%.o)
endif


DEP_SPARC =
DEP_SPARC += $(wildcard arch/Sparc/Sparc*.inc)

LIBOBJ_SPARC =
ifneq (,$(findstring sparc,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_SPARC
	LIBSRC_SPARC += $(wildcard arch/Sparc/Sparc*.c)
	LIBOBJ_SPARC += $(LIBSRC_SPARC:%.c=$(OBJDIR)/%.o)
endif


DEP_SYSZ =
DEP_SYSZ += $(wildcard arch/SystemZ/SystemZ*.inc)

LIBOBJ_SYSZ =
ifneq (,$(findstring systemz,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_SYSZ
	LIBSRC_SYSZ += $(wildcard arch/SystemZ/SystemZ*.c)
	LIBOBJ_SYSZ += $(LIBSRC_SYSZ:%.c=$(OBJDIR)/%.o)
endif


# by default, we compile full X86 instruction sets
X86_REDUCE =
ifneq (,$(findstring yes,$(CAPSTONE_X86_REDUCE)))
X86_REDUCE = _reduce
CFLAGS += -DCAPSTONE_X86_REDUCE -Os
endif

DEP_X86 =
DEP_X86 += arch/X86/X86GenAsmWriter$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86GenAsmWriter1$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86GenDisassemblerTables$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86GenInstrInfo$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86GenRegisterInfo.inc
DEP_X86 += arch/X86/X86MappingInsn$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86MappingInsnOp$(X86_REDUCE).inc
DEP_X86 += arch/X86/X86ImmSize.inc

LIBOBJ_X86 =
ifneq (,$(findstring x86,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_X86
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86DisassemblerDecoder.o
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86Disassembler.o
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86IntelInstPrinter.o
# assembly syntax is irrelevant in Diet mode, when this info is suppressed
ifeq (,$(findstring yes,$(CAPSTONE_DIET)))
ifeq (,$(findstring yes,$(CAPSTONE_X86_ATT_DISABLE)))
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86ATTInstPrinter.o
endif
endif
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86Mapping.o
	LIBOBJ_X86 += $(OBJDIR)/arch/X86/X86Module.o
endif


DEP_XCORE =
DEP_XCORE += $(wildcard arch/XCore/XCore*.inc)

LIBOBJ_XCORE =
ifneq (,$(findstring xcore,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_XCORE
	LIBSRC_XCORE += $(wildcard arch/XCore/XCore*.c)
	LIBOBJ_XCORE += $(LIBSRC_XCORE:%.c=$(OBJDIR)/%.o)
endif


DEP_TMS320C64X =
DEP_TMS320C64X += $(wildcard arch/TMS320C64x/TMS320C64x*.inc)

LIBOBJ_TMS320C64X =
ifneq (,$(findstring tms320c64x,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_TMS320C64X
	LIBSRC_TMS320C64X += $(wildcard arch/TMS320C64x/TMS320C64x*.c)
	LIBOBJ_TMS320C64X += $(LIBSRC_TMS320C64X:%.c=$(OBJDIR)/%.o)
endif

DEP_M680X =
DEP_M680X += $(wildcard arch/M680X/*.inc)
DEP_M680X += $(wildcard arch/M680X/M680X*.h)

LIBOBJ_M680X =
ifneq (,$(findstring m680x,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_M680X
	LIBSRC_M680X += $(wildcard arch/M680X/*.c)
	LIBOBJ_M680X += $(LIBSRC_M680X:%.c=$(OBJDIR)/%.o)
endif


DEP_EVM =
DEP_EVM += $(wildcard arch/EVM/EVM*.inc)

LIBOBJ_EVM =
ifneq (,$(findstring evm,$(CAPSTONE_ARCHS)))
	CFLAGS += -DCAPSTONE_HAS_EVM
	LIBSRC_EVM += $(wildcard arch/EVM/EVM*.c)
	LIBOBJ_EVM += $(LIBSRC_EVM:%.c=$(OBJDIR)/%.o)
endif


LIBOBJ =
LIBOBJ += $(OBJDIR)/cs.o $(OBJDIR)/utils.o $(OBJDIR)/SStream.o $(OBJDIR)/MCInstrDesc.o $(OBJDIR)/MCRegisterInfo.o
LIBOBJ += $(LIBOBJ_ARM) $(LIBOBJ_ARM64) $(LIBOBJ_M68K) $(LIBOBJ_MIPS) $(LIBOBJ_PPC) $(LIBOBJ_SPARC) $(LIBOBJ_SYSZ) $(LIBOBJ_X86) $(LIBOBJ_XCORE) $(LIBOBJ_TMS320C64X) $(LIBOBJ_M680X) $(LIBOBJ_EVM)
LIBOBJ += $(OBJDIR)/MCInst.o


ifeq ($(PKG_EXTRA),)
PKGCFGDIR = $(LIBDATADIR)/pkgconfig
else
PKGCFGDIR ?= $(LIBDATADIR)/pkgconfig
ifeq ($(PKGCFGDIR),)
PKGCFGDIR = $(LIBDATADIR)/pkgconfig
endif
endif

API_MAJOR=$(shell echo `grep -e CS_API_MAJOR include/capstone/capstone.h | grep -v = | awk '{print $$3}'` | awk '{print $$1}')
VERSION_EXT =

IS_APPLE := $(shell $(CC) -dM -E - < /dev/null 2> /dev/null | grep __apple_build_version__ | wc -l | tr -d " ")
ifeq ($(IS_APPLE),1)
# on MacOS, compile in Universal format by default
MACOS_UNIVERSAL ?= yes
ifeq ($(MACOS_UNIVERSAL),yes)
CFLAGS += $(foreach arch,$(LIBARCHS),-arch $(arch))
LDFLAGS += $(foreach arch,$(LIBARCHS),-arch $(arch))
endif
EXT = dylib
VERSION_EXT = $(API_MAJOR).$(EXT)
$(LIBNAME)_LDFLAGS += -dynamiclib -install_name lib$(LIBNAME).$(VERSION_EXT) -current_version $(PKG_MAJOR).$(PKG_MINOR).$(PKG_EXTRA) -compatibility_version $(PKG_MAJOR).$(PKG_MINOR)
AR_EXT = a
# Homebrew wants to make sure its formula does not disable FORTIFY_SOURCE
# However, this is not really necessary because 'CAPSTONE_USE_SYS_DYN_MEM=yes' by default
ifneq ($(HOMEBREW_CAPSTONE),1)
ifneq ($(CAPSTONE_USE_SYS_DYN_MEM),yes)
# remove string check because OSX kernel complains about missing symbols
CFLAGS += -D_FORTIFY_SOURCE=0
endif
endif
else
CFLAGS += $(foreach arch,$(LIBARCHS),-arch $(arch))
LDFLAGS += $(foreach arch,$(LIBARCHS),-arch $(arch))
$(LIBNAME)_LDFLAGS += -shared
# Cygwin?
IS_CYGWIN := $(shell $(CC) -dumpmachine 2>/dev/null | grep -i cygwin | wc -l)
ifeq ($(IS_CYGWIN),1)
EXT = dll
AR_EXT = lib
# Cygwin doesn't like -fPIC
CFLAGS := $(CFLAGS:-fPIC=)
# On Windows we need the shared library to be executable
else
# mingw?
IS_MINGW := $(shell $(CC) --version 2>/dev/null | grep -i mingw | wc -l)
ifeq ($(IS_MINGW),1)
EXT = dll
AR_EXT = lib
# mingw doesn't like -fPIC either
CFLAGS := $(CFLAGS:-fPIC=)
# On Windows we need the shared library to be executable
else
# Linux, *BSD
EXT = so
VERSION_EXT = $(EXT).$(API_MAJOR)
AR_EXT = a
$(LIBNAME)_LDFLAGS += -Wl,-soname,lib$(LIBNAME).$(VERSION_EXT)
endif
endif
endif

ifeq ($(CAPSTONE_SHARED),yes)
ifeq ($(IS_MINGW),1)
LIBRARY = $(BLDIR)/$(LIBNAME).$(VERSION_EXT)
else ifeq ($(IS_CYGWIN),1)
LIBRARY = $(BLDIR)/$(LIBNAME).$(VERSION_EXT)
else	# *nix
LIBRARY = $(BLDIR)/lib$(LIBNAME).$(VERSION_EXT)
CFLAGS += -fvisibility=hidden
endif
endif

ifeq ($(CAPSTONE_STATIC),yes)
ifeq ($(IS_MINGW),1)
ARCHIVE = $(BLDIR)/$(LIBNAME).$(AR_EXT)
else ifeq ($(IS_CYGWIN),1)
ARCHIVE = $(BLDIR)/$(LIBNAME).$(AR_EXT)
else
ARCHIVE = $(BLDIR)/lib$(LIBNAME).$(AR_EXT)
endif
endif

PKGCFGF = $(BLDIR)/$(LIBNAME).pc

.PHONY: all clean install uninstall dist

all: $(LIBRARY) $(ARCHIVE) $(PKGCFGF)
ifeq (,$(findstring yes,$(CAPSTONE_BUILD_CORE_ONLY)))
	@V=$(V) CC=$(CC) $(MAKE) -C cstool
ifndef BUILDDIR
	cd tests && $(MAKE)
else
	cd tests && $(MAKE) BUILDDIR=$(BLDIR)
endif
	$(call install-library,$(BLDIR)/tests/)
endif

ifeq ($(CAPSTONE_SHARED),yes)
$(LIBRARY): $(LIBOBJ)
ifeq ($(V),0)
	$(call log,LINK,$(@:$(BLDIR)/%=%))
	@$(create-library)
else
	$(create-library)
endif
endif

$(LIBOBJ): config.mk *.h include/capstone/*.h

$(LIBOBJ_ARM): $(DEP_ARM)
$(LIBOBJ_ARM64): $(DEP_ARM64)
$(LIBOBJ_M68K): $(DEP_M68K)
$(LIBOBJ_MIPS): $(DEP_MIPS)
$(LIBOBJ_PPC): $(DEP_PPC)
$(LIBOBJ_SPARC): $(DEP_SPARC)
$(LIBOBJ_SYSZ): $(DEP_SYSZ)
$(LIBOBJ_X86): $(DEP_X86)
$(LIBOBJ_XCORE): $(DEP_XCORE)
$(LIBOBJ_TMS320C64X): $(DEP_TMS320C64X)
$(LIBOBJ_M680X): $(DEP_M680X)
$(LIBOBJ_EVM): $(DEP_EVM)

ifeq ($(CAPSTONE_STATIC),yes)
$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
ifeq ($(V),0)
	$(call log,AR,$(@:$(BLDIR)/%=%))
	@$(create-archive)
else
	$(create-archive)
endif
endif

$(PKGCFGF):
ifeq ($(V),0)
	$(call log,GEN,$(@:$(BLDIR)/%=%))
	@$(generate-pkgcfg)
else
	$(generate-pkgcfg)
endif

install: $(PKGCFGF) $(ARCHIVE) $(LIBRARY)
	mkdir -p $(DESTDIR)$(LIBDIR)
	$(call install-library,$(DESTDIR)$(LIBDIR))
ifeq ($(CAPSTONE_STATIC),yes)
	$(INSTALL_DATA) $(ARCHIVE) $(DESTDIR)$(LIBDIR)
endif
	mkdir -p $(DESTDIR)$(INCDIR)/$(LIBNAME)
	$(INSTALL_DATA) include/capstone/*.h $(DESTDIR)$(INCDIR)/$(LIBNAME)
	mkdir -p $(DESTDIR)$(PKGCFGDIR)
	$(INSTALL_DATA) $(PKGCFGF) $(DESTDIR)$(PKGCFGDIR)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_LIB) cstool/cstool $(DESTDIR)$(BINDIR)

uninstall:
	rm -rf $(DESTDIR)$(INCDIR)/$(LIBNAME)
	rm -f $(DESTDIR)$(LIBDIR)/lib$(LIBNAME).*
	rm -f $(DESTDIR)$(PKGCFGDIR)/$(LIBNAME).pc
	rm -f $(DESTDIR)$(BINDIR)/cstool

clean:
	rm -f $(LIBOBJ)
	rm -f $(BLDIR)/lib$(LIBNAME).* $(BLDIR)/$(LIBNAME).pc
	rm -f $(PKGCFGF)
	$(MAKE) -C cstool clean

ifeq (,$(findstring yes,$(CAPSTONE_BUILD_CORE_ONLY)))
	cd tests && $(MAKE) clean
	rm -f $(BLDIR)/tests/lib$(LIBNAME).$(EXT)
endif

ifdef BUILDDIR
	rm -rf $(BUILDDIR)
endif

ifeq (,$(findstring yes,$(CAPSTONE_BUILD_CORE_ONLY)))
	cd bindings/python && $(MAKE) clean
	cd bindings/java && $(MAKE) clean
	cd bindings/ocaml && $(MAKE) clean
endif


TAG ?= HEAD
ifeq ($(TAG), HEAD)
DIST_VERSION = latest
else
DIST_VERSION = $(TAG)
endif

dist:
	git archive --format=tar.gz --prefix=capstone-$(DIST_VERSION)/ $(TAG) > capstone-$(DIST_VERSION).tgz
	git archive --format=zip --prefix=capstone-$(DIST_VERSION)/ $(TAG) > capstone-$(DIST_VERSION).zip


TESTS = test_basic test_detail test_arm test_arm64 test_m68k test_mips test_ppc test_sparc
TESTS += test_systemz test_x86 test_xcore test_iter
TESTS += test_basic.static test_detail.static test_arm.static test_arm64.static
TESTS += test_m68k.static test_mips.static test_ppc.static test_sparc.static
TESTS += test_systemz.static test_x86.static test_xcore.static test_m680x.static
TESTS += test_skipdata test_skipdata.static test_iter.static
check:
	@for t in $(TESTS); do \
		echo Check $$t ... ; \
		LD_LIBRARY_PATH=./tests ./tests/$$t > /dev/null && echo OK || echo FAILED; \
	done

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
ifeq ($(V),0)
	$(call log,CC,$(@:$(OBJDIR)/%=%))
	@$(compile)
else
	$(compile)
endif


ifeq ($(CAPSTONE_SHARED),yes)
define install-library
	$(INSTALL_LIB) $(LIBRARY) $1
	$(if $(VERSION_EXT),
		cd $1 && \
		rm -f lib$(LIBNAME).$(EXT) && \
		ln -s lib$(LIBNAME).$(VERSION_EXT) lib$(LIBNAME).$(EXT))
endef
else
define install-library
endef
endif


define create-archive
	$(AR) q $(ARCHIVE) $(LIBOBJ)
	$(RANLIB) $(ARCHIVE)
endef


define create-library
	$(CC) $(LDFLAGS) $($(LIBNAME)_LDFLAGS) $(LIBOBJ) -o $(LIBRARY)
endef


define generate-pkgcfg
	mkdir -p $(BLDIR)
	echo 'Name: capstone' > $(PKGCFGF)
	echo 'Description: Capstone disassembly engine' >> $(PKGCFGF)
	echo 'Version: $(PKG_VERSION)' >> $(PKGCFGF)
	echo 'libdir=$(LIBDIR)' >> $(PKGCFGF)
	echo 'includedir=$(INCDIR)' >> $(PKGCFGF)
	echo 'archive=$${libdir}/libcapstone.a' >> $(PKGCFGF)
	echo 'Libs: -L$${libdir} -lcapstone' >> $(PKGCFGF)
	echo 'Cflags: -I$${includedir}' >> $(PKGCFGF)
endef

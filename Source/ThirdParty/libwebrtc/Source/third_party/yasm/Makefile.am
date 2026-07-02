SUBDIRS = po .

ACLOCAL_AMFLAGS = -I m4
AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@

bin_PROGRAMS =
dist_man_MANS =
notrans_dist_man_MANS =
TESTS =
TESTS_ENVIRONMENT =
noinst_PROGRAMS = genstring

check_PROGRAMS = test_hd

test_hd_SOURCES = test_hd.c

include_HEADERS = libyasm.h
nodist_include_HEADERS = libyasm-stdint.h

noinst_HEADERS = util.h

BUILT_SOURCES =
MAINTAINERCLEANFILES =
DISTCLEANFILES =
SUFFIXES =

# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath

# libyasm-stdint.h doesn't clean up after itself?
CONFIG_CLEAN_FILES = libyasm-stdint.h
CONFIG_CLEAN_FILES += YASM-VERSION-FILE
CONFIG_CLEAN_FILES += YASM-VERSION.h

EXTRA_DIST += tools/Makefile.inc
EXTRA_DIST += libyasm/Makefile.inc
EXTRA_DIST += modules/Makefile.inc
EXTRA_DIST += frontends/Makefile.inc

include tools/Makefile.inc

YASM_MODULES = 
lib_LIBRARIES = libyasm.a
libyasm_a_SOURCES =
nodist_libyasm_a_SOURCES =
include modules/Makefile.inc

include libyasm/Makefile.inc
include frontends/Makefile.inc
include m4/Makefile.inc

EXTRA_DIST += out_test.sh
EXTRA_DIST += Artistic.txt
EXTRA_DIST += BSD.txt
EXTRA_DIST += GNU_GPL-2.0
EXTRA_DIST += GNU_LGPL-2.0
EXTRA_DIST += splint.sh
EXTRA_DIST += YASM-VERSION-GEN.sh
EXTRA_DIST += CMakeLists.txt
EXTRA_DIST += ConfigureChecks.cmake
EXTRA_DIST += config.h.cmake
EXTRA_DIST += libyasm-stdint.h.cmake
EXTRA_DIST += cmake/CMakeLists.txt
EXTRA_DIST += cmake/modules/CMakeLists.txt
EXTRA_DIST += cmake/modules/DummyCFile.c
EXTRA_DIST += cmake/modules/VersionGen.cmake
EXTRA_DIST += cmake/modules/YasmMacros.cmake
EXTRA_DIST += frontends/CMakeLists.txt
EXTRA_DIST += frontends/tasm/CMakeLists.txt
EXTRA_DIST += frontends/vsyasm/CMakeLists.txt
EXTRA_DIST += frontends/yasm/CMakeLists.txt
EXTRA_DIST += frontends/yasm/genstring.py
EXTRA_DIST += frontends/yasm/yasm-plugin.c
EXTRA_DIST += frontends/yasm/yasm-plugin.h
EXTRA_DIST += libyasm/CMakeLists.txt
EXTRA_DIST += libyasm/cmake-module.c
EXTRA_DIST += modules/arch/CMakeLists.txt
EXTRA_DIST += modules/arch/lc3b/CMakeLists.txt
EXTRA_DIST += modules/arch/x86/CMakeLists.txt
EXTRA_DIST += modules/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/codeview/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/dwarf2/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/null/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/stabs/CMakeLists.txt
EXTRA_DIST += modules/listfmts/CMakeLists.txt
EXTRA_DIST += modules/listfmts/nasm/CMakeLists.txt
EXTRA_DIST += modules/objfmts/bin/CMakeLists.txt
EXTRA_DIST += modules/objfmts/CMakeLists.txt
EXTRA_DIST += modules/objfmts/coff/CMakeLists.txt
EXTRA_DIST += modules/objfmts/dbg/CMakeLists.txt
EXTRA_DIST += modules/objfmts/elf/CMakeLists.txt
EXTRA_DIST += modules/objfmts/macho/CMakeLists.txt
EXTRA_DIST += modules/objfmts/rdf/CMakeLists.txt
EXTRA_DIST += modules/objfmts/xdf/CMakeLists.txt
EXTRA_DIST += modules/parsers/CMakeLists.txt
EXTRA_DIST += modules/parsers/gas/CMakeLists.txt
EXTRA_DIST += modules/parsers/nasm/CMakeLists.txt
EXTRA_DIST += modules/preprocs/CMakeLists.txt
EXTRA_DIST += modules/preprocs/cpp/CMakeLists.txt
EXTRA_DIST += modules/preprocs/gas/CMakeLists.txt
EXTRA_DIST += modules/preprocs/nasm/CMakeLists.txt
EXTRA_DIST += modules/preprocs/raw/CMakeLists.txt
EXTRA_DIST += plugins/README
EXTRA_DIST += plugins/dbg/CMakeLists.txt
EXTRA_DIST += plugins/dbg/dbg-objfmt.c
EXTRA_DIST += plugins/dbg/init_plugin.c
EXTRA_DIST += plugins/dbg/README
EXTRA_DIST += plugins/x86/CMakeLists.txt
EXTRA_DIST += plugins/x86/init_plugin.c
EXTRA_DIST += plugins/x86/README
EXTRA_DIST += tools/CMakeLists.txt
EXTRA_DIST += tools/genmacro/CMakeLists.txt
EXTRA_DIST += tools/genperf/CMakeLists.txt
EXTRA_DIST += tools/re2c/CMakeLists.txt
EXTRA_DIST += Mkfiles/Makefile.flat
EXTRA_DIST += Mkfiles/Makefile.dj
EXTRA_DIST += Mkfiles/dj/config.h
EXTRA_DIST += Mkfiles/dj/libyasm-stdint.h
EXTRA_DIST += Mkfiles/vc9/config.h
EXTRA_DIST += Mkfiles/vc9/crt_secure_no_deprecate.vsprops
EXTRA_DIST += Mkfiles/vc9/libyasm-stdint.h
EXTRA_DIST += Mkfiles/vc9/readme.vc9.txt
EXTRA_DIST += Mkfiles/vc9/vc98_swap.py
EXTRA_DIST += Mkfiles/vc9/vsyasm.vcproj
EXTRA_DIST += Mkfiles/vc9/yasm.sln
EXTRA_DIST += Mkfiles/vc9/yasm.vcproj
EXTRA_DIST += Mkfiles/vc9/ytasm.vcproj
EXTRA_DIST += Mkfiles/vc9/yasm.rules
EXTRA_DIST += Mkfiles/vc9/genmacro/genmacro.vcproj
EXTRA_DIST += Mkfiles/vc9/genmacro/run.bat
EXTRA_DIST += Mkfiles/vc9/genmodule/genmodule.vcproj
EXTRA_DIST += Mkfiles/vc9/genmodule/run.bat
EXTRA_DIST += Mkfiles/vc9/genperf/genperf.vcproj
EXTRA_DIST += Mkfiles/vc9/genperf/run.bat
EXTRA_DIST += Mkfiles/vc9/genstring/genstring.vcproj
EXTRA_DIST += Mkfiles/vc9/genstring/run.bat
EXTRA_DIST += Mkfiles/vc9/genversion/genversion.vcproj
EXTRA_DIST += Mkfiles/vc9/genversion/run.bat
EXTRA_DIST += Mkfiles/vc9/libyasm/libyasm.vcproj
EXTRA_DIST += Mkfiles/vc9/modules/modules.vcproj
EXTRA_DIST += Mkfiles/vc9/re2c/re2c.vcproj
EXTRA_DIST += Mkfiles/vc9/re2c/run.bat
EXTRA_DIST += Mkfiles/vc10/config.h
EXTRA_DIST += Mkfiles/vc10/crt_secure_no_deprecate.props
EXTRA_DIST += Mkfiles/vc10/crt_secure_no_deprecate.vsprops
EXTRA_DIST += Mkfiles/vc10/libyasm-stdint.h
EXTRA_DIST += Mkfiles/vc10/out_copy_rename.bat
EXTRA_DIST += Mkfiles/vc10/readme.vc10.txt
EXTRA_DIST += Mkfiles/vc10/vsyasm.props
EXTRA_DIST += Mkfiles/vc10/vsyasm.targets
EXTRA_DIST += Mkfiles/vc10/vsyasm.vcxproj
EXTRA_DIST += Mkfiles/vc10/vsyasm.xml
EXTRA_DIST += Mkfiles/vc10/yasm.sln
EXTRA_DIST += Mkfiles/vc10/yasm.vcxproj
EXTRA_DIST += Mkfiles/vc10/yasm.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/ytasm.vcxproj
EXTRA_DIST += Mkfiles/vc10/genmacro/genmacro.vcxproj
EXTRA_DIST += Mkfiles/vc10/genmacro/genmacro.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/genmacro/run.bat
EXTRA_DIST += Mkfiles/vc10/genmodule/genmodule.vcxproj
EXTRA_DIST += Mkfiles/vc10/genmodule/genmodule.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/genmodule/run.bat
EXTRA_DIST += Mkfiles/vc10/genperf/genperf.vcxproj
EXTRA_DIST += Mkfiles/vc10/genperf/genperf.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/genperf/run.bat
EXTRA_DIST += Mkfiles/vc10/genstring/genstring.vcxproj
EXTRA_DIST += Mkfiles/vc10/genstring/genstring.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/genstring/run.bat
EXTRA_DIST += Mkfiles/vc10/genversion/genversion.vcxproj
EXTRA_DIST += Mkfiles/vc10/genversion/genversion.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/genversion/run.bat
EXTRA_DIST += Mkfiles/vc10/libyasm/libyasm.vcxproj
EXTRA_DIST += Mkfiles/vc10/libyasm/libyasm.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/modules/modules.vcxproj
EXTRA_DIST += Mkfiles/vc10/modules/modules.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/re2c/re2c.vcxproj
EXTRA_DIST += Mkfiles/vc10/re2c/re2c.vcxproj.filters
EXTRA_DIST += Mkfiles/vc10/re2c/run.bat
EXTRA_DIST += Mkfiles/vc12/config.h
EXTRA_DIST += Mkfiles/vc12/crt_secure_no_deprecate.props
EXTRA_DIST += Mkfiles/vc12/crt_secure_no_deprecate.vsprops
EXTRA_DIST += Mkfiles/vc12/libyasm/libyasm.vcxproj
EXTRA_DIST += Mkfiles/vc12/libyasm/libyasm.vcxproj.filters
EXTRA_DIST += Mkfiles/vc12/modules/modules.vcxproj
EXTRA_DIST += Mkfiles/vc12/modules/modules.vcxproj.filters
EXTRA_DIST += Mkfiles/vc12/out_copy_rename.bat
EXTRA_DIST += Mkfiles/vc12/readme.vc12.txt
EXTRA_DIST += Mkfiles/vc12/vsyasm.props
EXTRA_DIST += Mkfiles/vc12/vsyasm.targets
EXTRA_DIST += Mkfiles/vc12/vsyasm.vcxproj
EXTRA_DIST += Mkfiles/vc12/vsyasm.xml
EXTRA_DIST += Mkfiles/vc12/yasm.sln
EXTRA_DIST += Mkfiles/vc12/yasm.vcxproj
EXTRA_DIST += Mkfiles/vc12/yasm.vcxproj.filters
EXTRA_DIST += Mkfiles/vc12/ytasm.vcxproj

# Until this gets fixed in automake
DISTCLEANFILES += libyasm/stamp-h libyasm/stamp-h[0-9]*

dist-hook: YASM-VERSION-FILE YASM-VERSION.h
	cp YASM-VERSION-FILE $(distdir)/version
	cp YASM-VERSION.h $(distdir)/Mkfiles/dj/
	cp YASM-VERSION.h $(distdir)/Mkfiles/vc9/
	cp YASM-VERSION.h $(distdir)/Mkfiles/vc10/
	cp YASM-VERSION.h $(distdir)/Mkfiles/vc12/

YASM-VERSION-FILE: $(top_srcdir)/YASM-VERSION-GEN.sh
	$(top_srcdir)/YASM-VERSION-GEN.sh

distclean-local:
	-rm -rf results
if HAVE_PYTHON
	-rm -rf build
endif

# Until gets fixed libyasm cross-build for target system
# use fixinstall-yasm-XXX make targets
all-local: python-build
install-exec-hook: python-install
uninstall-hook: python-uninstall

if BUILD_MAN
MAINTAINERCLEANFILES += $(dist_man_MANS) $(notrans_dist_man_MANS)
endif

# genstring build
genstring_SOURCES =
EXTRA_DIST += genstring.c
genstring_LDADD = genstring.$(OBJEXT)
genstring_LINK = $(CCLD_FOR_BUILD) -o $@

genstring.$(OBJEXT): genstring.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f genstring.c || echo '$(srcdir)/'`genstring.c


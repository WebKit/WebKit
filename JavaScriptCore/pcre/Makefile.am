
noinst_PROGRAMS = dftables

dftables_SRCS = dftables.c

chartables.c: dftables
	./dftables > $(srcdir)/chartables.c

BUILT_SOURCES = chartables.c

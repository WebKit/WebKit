#    This file is part of the KDE libraries
#
#
#    Copyright (C) 1996-1997 Matthias Kalle Dalheimer (kalle@kde.org)
#			  (C) 1997 Stephan Kulow (coolo@kde.org)

noinst_HEADERS = kentities.c

BUILT_SOURCES = $(noinst_HEADERS)

kentities.c: kentities.gperf
	$(GPERF) -a -L "ANSI-C" -C -G -c -o -t -k '*' -NfindEntity -D -s 2 $< > $@

CLEANFILES = $(BUILT_SOURCES)
lib_LTLIBRARIES = libkjava.la
libkjava_la_SOURCES = kjavaapplet.cpp kjavaappletcontext.cpp \
	kjavaappletserver.cpp kjavaappletwidget.cpp kjavaprocess.cpp \
	kjavadownloader.cpp javaembed.cpp

noinst_HEADERS = javaembed.h

libkjava_la_METASOURCES = AUTO

include_HEADERS = kjavaapplet.h kjavaappletcontext.h \
	kjavaappletserver.h kjavaappletwidget.h kjavaprocess.h

libkjava_la_LDFLAGS = -version-info 1:0 -no-undefined
libkjava_la_LIBADD = $(LIB_KDECORE) $(LIB_KIO)

INCLUDES = -I$(top_srcdir) $(all_includes) 

data_DATA = kjava.jar kjava.policy
datadir = $(kde_datadir)/kjava/

SUBDIRS = . tests

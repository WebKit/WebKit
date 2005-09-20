#!/usr/bin/env bash
#
# Helper script to invoke kdomidl.pl from Makefile.am
#
# Copyright (c) 2005 Nikolas Zimmermann <wildfox@kde.org>
#

# Expects following passed parameters:
SRCDIR=$1	# 1 - $(top_srcdir) variable from Makefile.am (ie. '.')
LANG=$2		# 2 - Language generator string identifier (ie. 'cpp')
MODULE=$3	# 3 - DOM module where IDLs live in (ie. 'core')
INPUT=$4	# 4 - Interface to process (ie. 'Node.cpp')
BUILDDIR=$5 # 5 - $(top_builddir) variable from Makefile.am (i.e. '.')

IDL=`basename $INPUT | sed 's/\.cpp/.idl/g;s/\.h/.idl/g' | sed 's/Wrapper//'`

test -f $BUILDDIR/kdom/bindings/$LANG/$MODULE/$INPUT \
	&& ! test $SRCDIR/kdom/bindings/idl/$MODULE/$IDL -nt $BUILDDIR/kdom/bindings/$LANG/$MODULE/$INPUT \
	&& exit 0
	
/usr/bin/env perl -I$SRCDIR/kdom/bindings $SRCDIR/kdom/bindings/kdomidl.pl \
		  --generator $LANG --outputdir $BUILDDIR/kdom/bindings/$LANG \
		  --input $SRCDIR/kdom/bindings/idl/$MODULE/$IDL \
		  --includedir $SRCDIR/kdom/bindings/idl \
		  --documentation $SRCDIR/kdom/bindings/idl/$MODULE/docs-$MODULE.xml

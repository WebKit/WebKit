#!/usr/bin/bash

# Copyright (C) 2007 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

SRCPATH=`cygpath -u "$WEBKITLIBRARIESDIR\Tools\Scripts"`
VERSIONPATH=`cygpath -u "$1"`
VERSIONPATH=$VERSIONPATH/include
VERSIONFILE=$VERSIONPATH/autoversion.h
mkdir -p "$VERSIONPATH"

if [ "$RC_PROJECTSOURCEVERSION" == "" ]; then
    PROPOSEDVERSION=`cat $SRCPATH/VERSION`
else
    PROPOSEDVERSION="$RC_PROJECTSOURCEVERSION"
fi
PROPOSEDVERSION=`echo "$PROPOSEDVERSION" | sed -r 's/(.+?)[\r\n]*$/\1/'`

BLDMAJORVERSION=`echo "$PROPOSEDVERSION" | sed 's/\([^\.]*\)\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\1/'`
BLDMINORVERSION=`echo "$PROPOSEDVERSION" | sed 's/\([^\.]*\)\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\3/'`
BLDVARIANTVERSION=`echo "$PROPOSEDVERSION" | sed 's/\([^\.]*\)\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\5/'`
if [ "$BLDMINORVERSION" == "" ]; then
    BLDMINORVERSION=0
fi
if [ "$BLDVARIANTVERSION" == "" ]; then
    BLDVARIANTVERSION=0
fi
SVNOPENSOURCEREVISION=`svn info | grep '^Revision' | sed -r 's/^Revision: (.+?)[\r\n]*$/\1/'`

MAJORVERSION=`echo "$PROPOSEDVERSION" | sed 's/\(.\)[^\.]*\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\1/'`
MINORVERSION=`echo "$PROPOSEDVERSION" | sed 's/.\([^\.]*\)\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\1/'`
TINYVERSION=`echo "$PROPOSEDVERSION" | sed 's/\([^\.]*\)\(\.\([^.]*\)\(\.\([^.]*\)\)\?\)\?/\3/'`
if [ "$MINORVERSION" == "" ]; then
    MINORVERSION=0
fi
if [ "$TINYVERSION" == "" ]; then
    TINYVERSION=0
fi

BLDNMBR="$PROPOSEDVERSION"
BLDNMBRSHORT="$BLDNMBR"

if [ "$RC_PROJECTSOURCEVERSION" == "" ]; then
    BLDNMBRSHORT="$BLDNMBRSHORT+"
    BLDUSERNAME=`echo "$(whoami)" | sed -r 's/(.+?)[\r\n]*$/\1/'`
    BLDDATE=`echo "$(date)" | sed -r 's/(.+?)[\r\n]*$/\1/'`
    BLDNMBR="$BLDNMBRSHORT $BLDUSERNAME - $BLDDATE - r$SVNOPENSOURCEREVISION"
fi

cat > "$VERSIONFILE" <<EOF
#define __VERSION_TEXT__ "$BLDNMBR"
#define __BUILD_NUMBER__ "$BLDNMBR"
#define __BUILD_NUMBER_SHORT__ "$BLDNMBRSHORT"
#define __VERSION_MAJOR__ $MAJORVERSION
#define __VERSION_MINOR__ $MINORVERSION
#define __VERSION_TINY__ $TINYVERSION
#define __VERSION_BUILD__ $BLDVARIANTVERSION
#define __BUILD_NUMBER_MAJOR__ $BLDMAJORVERSION
#define __BUILD_NUMBER_MINOR__ $BLDMINORVERSION
#define __BUILD_NUMBER_VARIANT__ $BLDVARIANTVERSION
#define __SVN_REVISION__ $SVNREVISION
EOF

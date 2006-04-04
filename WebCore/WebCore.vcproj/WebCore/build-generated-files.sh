#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitOutputDirUnix=`cygpath -a -u "$2"`
WebKitOutputConfigDirUnix="${WebKitOutputDirUnix}/$1"

SRCROOT="`pwd`/../.."
SRCROOT=`realpath "$SRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SRCROOT=`cygpath -m -s "$SRCROOT"`
SRCROOT=`cygpath -u "$SRCROOT"`
export SRCROOT
# FIXME: Eventually win32 might wish to generate to the Debug/Release output directories.
export BUILT_PRODUCTS_DIR="$SRCROOT"
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/kjs/create_hash_table"

"$SRCROOT/generate-derived-sources" || exit 1

cd "$SRCROOT"
mkdir -p "${WebKitOutputConfigDirUnix}"

echo "Copying necessary dll files"

if [ ../libxml/bin/libxml2.dll -nt "$WebKitOutputConfigDirUnix/libxml2.dll" ]; then
    echo "Copying libxml2.dll"
    cp ../libxml/bin/libxml2.dll "$WebKitOutputConfigDirUnix" || exit 1
fi

if [ ../libxslt/bin/libxslt.dll -nt "$WebKitOutputConfigDirUnix/libxslt.dll" ]; then
    echo "Copying libxslt.dll"
    cp ../libxslt/bin/libxslt.dll "$WebKitOutputConfigDirUnix" || exit 1
fi

if [ ../iconv/bin/iconv.dll -nt "$WebKitOutputConfigDirUnix/iconv.dll" ]; then
    echo "Copying iconv.dll"
    cp ../iconv/bin/iconv.dll "$WebKitOutputConfigDirUnix" || exit 1
fi

if [ ../zlib/bin/zlib1.dll -nt "$WebKitOutputConfigDirUnix/zlib1.dll" ]; then
    echo "Copying zlib1.dll"
    cp ../zlib/bin/zlib1.dll "$WebKitOutputConfigDirUnix" || exit 1
fi


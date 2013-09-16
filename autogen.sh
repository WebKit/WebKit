#! /bin/sh

# Allow invocation from a separate build directory; in that case, we change
# to the source directory to run the auto*, then change back before running configure
srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir" || exit 1

touch README INSTALL

if test -z `which autoreconf`; then
    echo "Error: autoreconf not found, please install it."
    exit 1
fi
autoreconf --verbose --install -I Source/autotools $ACLOCAL_FLAGS|| exit $?
rm -rf autom4te.cache

cd "$ORIGDIR" || exit 1

if test -z "$NOCONFIGURE"; then
    "$srcdir"/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
fi


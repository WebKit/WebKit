#! /bin/sh

# Allow invocation from a separate build directory; in that case, we change
# to the source directory to run the auto*, then change back before running configure
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

LIBTOOLIZE_FLAGS="--force --automake"
ACLOCAL_FLAGS="-I autotools"
AUTOMAKE_FLAGS="--foreign --add-missing"

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have autoconf installed to compile $PROJECT."
    echo "Install the appropriate package for your distribution,"
    echo "or get the source tarball at http://ftp.gnu.org/gnu/autoconf/"
    DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have automake installed to compile $PROJECT."
    echo "Install the appropriate package for your distribution,"
    echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
    DIE=1
}

LIBTOOLIZE=libtoolize
($LIBTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
    LIBTOOLIZE=glibtoolize
    ($LIBTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have libtool installed to compile $PROJECT."
        echo "Install the appropriate package for your distribution,"
        echo "or get the source tarball at http://ftp.gnu.org/gnu/libtool/"
        DIE=1
    }
}

if test "$DIE" -eq 1; then
    exit 1
fi

rm -rf $top_srcdir/autom4te.cache

touch README INSTALL

aclocal $ACLOCAL_FLAGS || exit $?
$LIBTOOLIZE $LIBTOOLIZE_FLAGS || exit $?
autoheader || exit $?
automake $AUTOMAKE_FLAGS || exit $?
autoconf || exit $?

cd $ORIGDIR || exit 1

$srcdir/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?

#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd $srcdir

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/autoconf/"
	DIE=1
}

if automake-1.9 --version < /dev/null > /dev/null 2>&1 ; then
    AUTOMAKE=automake-1.9
    ACLOCAL=aclocal-1.9
else
	echo
	echo "You must have automake 1.9.x installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
	DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

rm -rf $top_srcdir/autom4te.cache

touch README INSTALL

$ACLOCAL || exit $?
libtoolize --force || exit $?
autoheader || exit $?
$AUTOMAKE --foreign --add-missing || exit $?
autoconf || exit $?

./configure --enable-maintainer-mode $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?

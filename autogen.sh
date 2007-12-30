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

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

rm -rf $top_srcdir/autom4te.cache

touch README INSTALL

aclocal || exit $?
libtoolize --force || exit $?
autoheader || exit $?
automake --foreign --add-missing || exit $?
autoconf || exit $?

./configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?

#!/bin/sh

YVF=YASM-VERSION-FILE
DEF_VER=v1.3.0

LF='
'

# First see if there is a version file (included in release tarballs),
# then try git-describe, then default.
if test -f version
then
	VN=$(cat version) || VN="$DEF_VER"
elif test -d .git -o -f .git &&
	VN=$(git describe --match "v[0-9]*" --abbrev=4 HEAD 2>/dev/null) &&
	case "$VN" in
	*$LF*) (exit 1) ;;
	v[0-9]*)
		git update-index -q --refresh
		test -z "$(git diff-index --name-only HEAD --)" ||
		VN="$VN-dirty" ;;
	esac
then
	VN=$(echo "$VN" | sed -e 's/-/./g');
else
	VN="$DEF_VER"
fi

VN=$(expr "$VN" : v*'\(.*\)')

if test -r $YVF
then
	VC=$(cat $YVF)
else
	VC=unset
fi
test "$VN" = "$VC" || {
	echo >&2 "$VN"
	echo "$VN" >$YVF
	echo "#define PACKAGE_STRING \"yasm $VN\"" > YASM-VERSION.h
	echo "#define PACKAGE_VERSION \"$VN\"" >> YASM-VERSION.h
}

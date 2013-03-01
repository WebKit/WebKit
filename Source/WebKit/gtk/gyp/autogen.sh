export SCRIPT_DIRECTORY=`dirname $0`
export TOPLEVEL_DIRECTORY="$SCRIPT_DIRECTORY/../../../.."

if [ ! -e Configuration.gypi.in ]; then
    ln -s "$SCRIPT_DIRECTORY/Configuration.gypi.in"
fi

if [ ! -e run-gyp ]; then
    ln -s "$SCRIPT_DIRECTORY/run-gyp"
fi

if [ ! -e configure.ac ]; then
    ln -s "$SCRIPT_DIRECTORY/configure.ac"
fi

if [ ! -e WebKitMacros ]; then
    ln -s "$TOPLEVEL_DIRECTORY/Source/autotools" WebKitMacros
fi

if [ ! -d Macros ]; then
    mkdir Macros
    cp  "$TOPLEVEL_DIRECTORY/Source/autotools/acinclude.m4" Macros
fi

if [ ! -d Tools ]; then
    mkdir Tools
fi

if [ ! -e Tools/gtk ]; then
    ln -s "../$TOPLEVEL_DIRECTORY/Tools/gtk" Tools/gtk
fi

autoreconf --verbose --install -I Macros $ACLOCAL_FLAGS

# Automake is bizarrely responsible for copying some of the files necessary
# for configure to run to the macros directory. So we invoke it here and hide
# the warnings we get about not using automake in configure.ac.
automake --add-missing --copy > /dev/null 2>&1

if test -z "$NOCONFIGURE"; then
    ./configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
fi

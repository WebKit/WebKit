# Copyright (c) 2018-2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e
set -x

config=Release
sdk=macosx.internal
archs=blank
variants=all
target=all
port=10022
testArgs=""

show_help() {
    set +x
    echo "$0 [-hcsavtpb] [test_args]"
    echo
    echo "Script options:"
    echo "-h                     Show help"
    echo "-c <config>            Set the config. Could be Release or Debug."
    echo "                       Default is $config."
    echo "-s <sdk>               Set the SDK. Could be macosx.internal or iphoneos.internal."
    echo "                       Default is $sdk."
    echo "-a <archs>             Set the archs. Default is blank (we don't set them)."
    echo "-v <variants>          Set the variants. Could be all, testing, or default."
    echo "                       Default is $variants."
    echo "-t <target>            Set the target. Could be all, pas, test_pas, mbmalloc,"
    echo "                       verifier, or chaos. Default is $target."
    echo "-p <port>              Set the localhost port to use for iOS on-device testing."
    echo "                       Default is $port."
    echo "--child-processes <n>  Set the maximum number of concurrent test-runner processes."
    echo "                       Default is nprocs / 2."
    exit 0
}

scriptArgs=""
inTestArgs=false

for arg in "$@"; do
    case "$arg" in
        --)
            # Explicit separator - everything after goes to test, overrides long/short check below
            inTestArgs=true
            ;;
        --*)
            # Long option - pass through to test binary by default
            testArgs="$testArgs $arg"
            inTestArgs=true
            ;;
        -*)
            # Short option - consume within this script by default
            if [ "$inTestArgs" = "true" ]; then
                testArgs="$testArgs $arg"
            else
                scriptArgs="$scriptArgs $arg"
            fi
            ;;
        *)
            # Non-option argument
            # If we've seen test args already, or if this looks like a test filter, add to test args
            # Otherwise, it's a script arg value (like the value for -c Release)
            if [ "$inTestArgs" = "true" ]; then
                testArgs="$testArgs $arg"
            else
                # This could be a value for a script option, or a test filter
                # We'll add it to scriptArgs and let getopts handle it
                scriptArgs="$scriptArgs $arg"
            fi
            ;;
    esac
done

# Now parse script args with getopts
# We need to reset positional parameters for getopts to work
eval set -- $scriptArgs

while getopts ":hc:s:a:v:t:p:" opt
do
    case $opt in
        h)
            show_help
            ;;
        c)
            config=$OPTARG
            ;;
        s)
            sdk=$OPTARG
            ;;
        a)
            archs=$OPTARG
            ;;
        v)
            variants=$OPTARG
            ;;
        t)
            target=$OPTARG
            ;;
        p)
            port=$OPTARG
            ;;
        \?)
            echo "Bad argument: -$OPTARG. Use -h for help."
            exit 1
            ;;
    esac
done

shift $((OPTIND -1))

# Any remaining non-option args after script options are test filters/args
if [ "x$*" != "x" ]
then
    testArgs="$testArgs $*"
fi

ios=no
configdir=$config

if echo "$sdk" | grep -q iphone
then
    ios=yes
    configdir=$config-iphoneos
fi

argsSansVariants="-c $config -s $sdk -a $archs -t $target -p $port"

args="$argsSansVariants -v $variants"

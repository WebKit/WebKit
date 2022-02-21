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

show_help() {
    set +x
    echo "$0 [-hcsavtpb]"
    echo
    echo "-h             Show help"
    echo "-c <config>    Set the config. Could be Release or Debug. Default is $config."
    echo "-s <sdk>       Set the SDK. Could be macosx.internal or iphoneos.internal."
    echo "               Default is $sdk."
    echo "-a <archs>     Set the archs. Default is blank (we don't set them)."
    echo "-v <variants>  Set the variants. Could be all, testing, or default. Default is"
    echo "               $variants."
    echo "-t <target>    Set the target. Could be all, pas, test_pas, mbmalloc, verifier,"
    echo "               or chaos. Default is $target."
    echo "-p <port>      Set the localhost port to use for iOS on-device testing. Default"
    echo "               is $port."
    exit 0
}

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
            echo "Bad argument. Use -h for help."
            exit 1
    esac
done

shift $((OPTIND -1))

if [ "x$*" != "x" ]
then
    echo "Bad argument. Use -h for help."
    exit 1
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

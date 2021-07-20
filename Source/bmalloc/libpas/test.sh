#!/bin/sh
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

. common.sh

function copy_variant {
    variant=$1
    
    tar -czvf /dev/stdout build-$sdk-$variant/$configdir test-impl.sh common.sh | \
        ssh -p $port root@localhost "cd /var/mobile/XcodeBuiltProducts && tar -xzvf /dev/stdin"
}

case $ios in
    no)
        ./test-impl.sh $args
        ;;
    yes)
        case $variants in
            all|testing)
                copy_variant testing
                ;;
        esac
        case $variants in
            all|default)
                copy_variant default
                ;;
        esac

        ssh -p $port root@localhost \
            "cd /var/mobile/XcodeBuiltProducts && ./test-impl.sh $args"
        ;;
    *)
        echo "Bad value of ios"
        exit 1
esac




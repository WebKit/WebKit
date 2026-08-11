#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------
#
# todo-CI: REMOVE THIS AFTER ENABLING runtests.py directly on CI

test_path=`dirname "$0"`

build_type=$1
# Accept -d or -t. If none was given (i.e. current CI), 
# search for the known paths
if [[ $build_type != "-d" && $build_type != "-t" ]]; then
    echo "Warning: You haven't provide either '-d' (debug) or '-t' (test)."
    echo "Warning: Searching for ch.."
    if [[ -f "$test_path/../BuildLinux/Debug/ch" ]]; then
        echo "Warning: Debug build was found"
        build_type="-d"
    elif [[ -f "$test_path/../BuildLinux/Test/ch" ]]; then
        echo "Warning: Test build was found"
        build_type="-t"
    elif [[ -f "$test_path/../BuildLinux/Release/ch" ]]; then
        # TEST flags are not enabled for release build
        # however we would like to test if the compiled binary
        # works or not
        CH="$test_path/../BuildLinux/Release/ch"
        echo "Warning: Release build was found"
        RES=$(${CH} $test_path/test/basics/hello.js)
        if [[ $RES =~ "Error :" ]]; then
            echo "FAILED"
            exit 1
        else
            echo "PASS"
            exit 0
        fi
    else
        echo 'Error: ch not found- exiting'
        exit 1
    fi
fi

"$test_path/runtests.py" $build_type --not-tag exclude_jenkins
if [[ $? != 0 ]]; then
    exit 1
fi

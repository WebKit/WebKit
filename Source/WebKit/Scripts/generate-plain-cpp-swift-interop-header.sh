#!/bin/bash
set -e

# Swift/C++ interop generates a header file to be included in C++ code that provides
# definitions of types encountered in Swift.
# There's built-in XCode functionality to generate this file.
# But we need to do it twice:
# - once, processing Swift code that pertains to objective-C[++] types to generate
#   a header for inclusion in objective-C++ code
# - once, processing only Swift code that does not involve objective-C[++ types,
#   to generate a header which can be safely included in pure C++ code.
# This shell script does the second thing. We use the built-in XCode action
# for the first.
# Working around rdar://152836730
# Known deficiencies here:
# - We do not attempt to provide all the #defines normally provided to C++ nor
#   the -Ds to Swift.
# - We do not provide a full include path.
# - There is no attempt at full dependency tracking (.d files etc.)
# This is mostly OK because right now the only header file included from Swift
# refers to only one other header. Once we expand beyond that, more sophistication
# will be needed.

/usr/bin/swiftc -typecheck -emit-clang-header-path "${SCRIPT_OUTPUT_FILE_0}" "${SCRIPT_INPUT_FILE_0}" -DENABLE_SWIFT_DEMO_URI_SCHEME \
    -I${SRCROOT}/Modules/Internal -I${SRCROOT} -cxx-interoperability-mode=default -Xcc -std=c++2b -module-name WebKit


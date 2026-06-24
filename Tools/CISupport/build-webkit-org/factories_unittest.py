# Copyright (C) 2011-2020 Apple Inc. All rights reserved.
# Copyright (C) 2021 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from . import loadConfig
import json
import os
import unittest

class TestExpectedBuildSteps(unittest.TestCase):

    expected_steps = {
        'Apple-Tahoe-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'compile-webkit',
            'trigger'
        ],
        'Apple-Tahoe-Release-AppleSilicon-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Tahoe-Release-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Tahoe-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'compile-webkit',
            'trigger'
        ],
        'Apple-Tahoe-Debug-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Tahoe-Debug-AppleSilicon-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Tahoe-Release-WK2-Perf': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'perf-test'
        ],
        'Apple-Tahoe-Release-AppleSilicon-Test262-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'test262-test'
        ],
        'Apple-Tahoe-LLINT-CLoop-BuildAndTest': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'compile-webkit',
            'webkit-jsc-cloop-test'
        ],
        'Apple-Tahoe-Release-WK2-Accessibility-Isolated-Tree-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions'
        ],
        'Apple-Tahoe-Release-World-Leaks-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'world-leaks-tests',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Tahoe-Safer-CPP-Checks': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'install-cmake',
            'install-ninja',
            'get-llvm-version',
            'print-clang-version',
            'checkout-llvm-project',
            'get-swift-tag-name',
            'print-swift-version',
            'checkout-swift-project',
            'update-swift-checkouts',
            'build-swift',
            'install-metal-toolchain',
            'scan-build'
        ],
        'Apple-Tahoe-Debug-WK2-Site-Isolation-Tree-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Sequoia-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'compile-webkit',
            'trigger'
        ],
        'Apple-Sequoia-Release-WK2-Site-Isolation-Tree-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests'
        ],
        'Apple-Sequoia-Release-AppleSilicon-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Sequoia-Release-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Sequoia-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'compile-webkit',
            'trigger'
        ],
        'Apple-Sequoia-Debug-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Sequoia-Debug-AppleSilicon-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'lldb-webkit-test',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-Sequoia-Release-Test262-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'test262-test'
        ],
        'Apple-Sequoia-Debug-Test262-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'test262-test'
        ],
        'Apple-Sequoia-AppleSilicon-O3-Debug-JSC-BuildAndTest': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'set-o3-optimization-level',
            'compile-jsc',
            'jscore-test'
        ],
        'Apple-Sequoia-AppleSilicon-Release-JSC-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'jscore-test'
        ],
        'Apple-Sequoia-Intel-Release-JSC-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'prune-coresymbolicationd-cache-if-too-large',
            'download-built-product',
            'extract-built-product',
            'jscore-test'
        ],
        'Apple-iOS-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'Apple-iOS-26-Simulator-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Apple-iOS-26-Simulator-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Apple-iOS-26-Simulator-Release-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-iOS-26-Simulator-Release-WK2-Site-Isolation-Tree-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests'
        ],
        'Apple-iOS-26-Simulator-Debug-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-iOS-26-Safer-CPP-Checks': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'install-cmake',
            'install-ninja',
            'get-llvm-version',
            'print-clang-version',
            'checkout-llvm-project',
            'get-swift-tag-name',
            'print-swift-version',
            'checkout-swift-project',
            'update-swift-checkouts',
            'build-swift',
            'install-metal-toolchain',
            'scan-build'
        ],
        'Apple-iPadOS-26-Simulator-Release-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-iPadOS-26-Simulator-Debug-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-visionOS-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'Apple-visionOS-26-Simulator-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Apple-visionOS-26-Simulator-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Apple-visionOS-26-Simulator-Release-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-visionOS-26-Simulator-Debug-WK2-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'trigger-crash-log-submission'
        ],
        'Apple-tvOS-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'Apple-tvOS-Simulator-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'Apple-watchOS-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'Apple-watchOS-Simulator-26-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'GTK-Linux-64-bit-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'generate-jsc-bundle',
            'trigger'
        ],
        'GTK-Linux-64-bit-Release-Clang-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit'
        ],
        'GTK-Linux-64-bit-Release-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'GTK-Linux-64-bit-Release-JS-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'jscore-test',
            'test262-test'
        ],
        'GTK-Linux-64-bit-Release-WebDriver-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'webdriver-test'
        ],
        'GTK-Linux-64-bit-Debug-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'GTK-Linux-64-bit-Release-Perf-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'trigger'
        ],
        'GTK-Linux-64-bit-Release-Perf': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'perf-test',
            'benchmark-test'
        ],
        'GTK-Linux-64-bit-Release-Debian-Stable-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'GTK-Linux-64-bit-Release-Ubuntu-LTS-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'GTK-Linux-64-bit-Release-Ubuntu-2204-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'GTK-Linux-64bit-Release-Packaging-Nightly': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'generate-minibrowser-bundle'
        ],
        'GTK-Linux-64bit-Release-Packaging-Nightly': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'generate-minibrowser-bundle'
        ],
        'GTK-Linux-64-bit-Release-GTK3-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit'
        ],
        'GTK-Linux-64-bit-Release-MVT-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'MVT-tests'
        ],
        'Windows-64-bit-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Windows-64-bit-Release-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'Windows-64-bit-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'trigger'
        ],
        'Windows-64-bit-Debug-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'PlayStation-Debug-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'PlayStation-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'JSCOnly-Linux-AArch64-Release': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-jsc',
            'jscore-test'
        ],
        'JSCOnly-Linux-ARMv7-Thumb2-Release': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-jsc',
            'jscore-test'
        ],
        'WPE-Linux-64-bit-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'trigger'
        ],
        'WPE-Linux-64-bit-Release-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'WPE-Linux-64-bit-Release-JS-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'jscore-test',
            'test262-test'
        ],
        'WPE-Linux-64-bit-Release-WebDriver-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'webdriver-test'
        ],
        'WPE-Linux-64-bit-Debug-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'WPE-Linux-64bit-Release-Packaging-Nightly': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'generate-minibrowser-bundle'
        ],
        'WPE-Linux-64-bit-Release-Non-Unified-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit'
        ],
        'WPE-Linux-64-bit-Release-Ubuntu-LTS-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'WPE-Linux-64-bit-Release-Ubuntu-2204-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'WPE-Linux-64-bit-Release-Clang-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit'
        ],
        'WPE-Linux-ARM32-bit-Release-Debian-Stable-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'WPE-Linux-ARM64-bit-Release-Debian-Stable-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit'
        ],
        'WPE-Linux-RPi4-32bits-Mesa-Release-Perf-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'check-if-deployed-cross-target-image-is-updated',
            'trigger'
        ],
        'WPE-Linux-RPi4-64bits-Mesa-Release-Perf-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'compile-webkit',
            'check-if-deployed-cross-target-image-is-updated',
            'trigger'
        ],
        'WPE-Linux-RPi4-32bits-Mesa-Release-Perf-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'check-if-running-cross-target-image-is-updated',
            'download-built-product',
            'extract-built-product',
            'benchmark-test'
        ],
        'WPE-Linux-RPi4-64bits-Mesa-Release-Perf-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'check-if-running-cross-target-image-is-updated',
            'download-built-product',
            'extract-built-product',
            'benchmark-test'
        ],
        'WPE-Linux-64-bit-Release-LibWebRTC-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit'
        ],
        'WPE-Linux-64-bit-Release-MVT-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'MVT-tests'
        ],
        'WPE-Linux-64-bit-Release-Legacy-API-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests'
        ],
        'WPE-Linux-ARM64-bit-Release-Build': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'trigger'
        ],
        'WPE-Linux-ARM64-bit-Release-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests'
        ],
        'WPE-Linux-ARM64-bit-Release-JS-Tests': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'jscore-test',
            'test262-test'
        ],
        'WPE-Linux-ARM64-bit-Release-v252-BuildAndTest': [
            'configure-build',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'show-identifier',
            'kill-old-processes',
            'delete-WebKitBuild-directory',
            'delete-stale-build-files',
            'jhbuild',
            'compile-webkit',
            'jscore-test',
            'layout-test',
            'dashboard-tests',
            'archive-test-results',
            'upload',
            'extract-test-results',
            'set-permissions',
            'run-api-tests',
            'webkitpy-test',
            'webkitperl-test',
            'bindings-generation-tests',
            'builtins-generator-tests',
            'test262-test',
            'webdriver-test',
            'MVT-tests'
        ]
    }

    def setUp(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        self.config = {}
        loadConfig.loadBuilderConfig(self.config, is_test_mode_enabled=True, master_prefix_path=cwd)

    def test_all_expected_steps(self):
        for builder in self.config['builders']:
            buildSteps = []
            for step in builder['factory'].steps:
                step_name = step.kwargs.get('name', step.step_class.name)
                buildSteps.append(step_name)
            self.assertTrue(builder['name'] in self.expected_steps, 'Missing expected steps for builder: %s\n Actual result is %s' % (builder['name'], buildSteps))
            self.assertListEqual(self.expected_steps[builder['name']], buildSteps, msg="Expected steps don't match for builder %s" % builder['name'])

    def test_unnecessary_expected_steps(self):
        builders = set()
        for builder in self.config['builders']:
            builders.add(builder['name'])
        for builder in self.expected_steps:
            self.assertTrue(builder in builders, "Builder %s doesn't exist, but has unnecessary expected steps" % builder)

    def test_unique_platform_for_build_product_upload(self):
        # A builder that compiles WebKit and triggers other builders uploads its build product so the triggered testers can download it.
        # The upload destination is derived (see CompileWebKit and ConfigureBuild in steps.py) from the build properties as:
        #   "{fullPlatform}-{archForUpload}-{configuration}"
        # where fullPlatform is the builder's "platform", archForUpload is the architectures joined with "-", and configuration is "release"/"debug".
        # The matching testers re-derive that same key to download the product, so if two uploading builders share the key their build products collide
        # in the same remote directory (see the bug fixed in 308273@main, where two "wpe" builders overwrote each other's release.zip on S3).
        # Note: the actual upload step is added at runtime by CompileWebKit (addStepsAfterCurrentStep, gated on the "triggers" property), so it is
        # not present in the static factory step list. We therefore identify the uploading builders by the condition that produces the upload:
        # they run a compile step and have triggers set.
        upload_key_to_builder = {}
        for builder in self.config['builders']:
            configure_kwargs = None
            compiles = False
            for step in builder['factory'].steps:
                step_name = step.kwargs.get('name', step.step_class.name)
                if step_name == 'configure-build':
                    configure_kwargs = step.kwargs
                if step_name.startswith('compile'):
                    compiles = True
            if configure_kwargs is None:
                continue
            triggers = configure_kwargs.get('triggers')
            if not (compiles and triggers):
                continue
            arch_for_upload = '-'.join(configure_kwargs['architecture'].split(' '))
            upload_key = '%s-%s-%s' % (configure_kwargs['platform'], arch_for_upload, configure_kwargs['configuration'])
            self.assertNotIn(
                upload_key, upload_key_to_builder,
                msg='Builders "%s" and "%s" both upload their build product to the same path '
                    '"%s" (derived from platform-architecture-configuration). Give one of them a '
                    'unique "platform" suffix so the build products do not collide (see 308273@main).'
                    % (upload_key_to_builder.get(upload_key), builder['name'], upload_key))
            upload_key_to_builder[upload_key] = builder['name']

    def test_triggered_testers_share_builder_platform(self):
        # This is the complement of test_unique_platform_for_build_product_upload:
        # every bot that gets trigerred should share the same platform keys than the builder,
        # so the download URI for the built-product matches the builder upload.
        cwd = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(cwd, 'config.json')) as config_json:
            raw_config = json.load(config_json)
        triggerable_builders = {
            scheduler['name']: scheduler.get('builderNames', [])
            for scheduler in raw_config['schedulers']
            if scheduler.get('type') == 'Triggerable'
        }

        info = {}
        for builder in self.config['builders']:
            configure_kwargs = None
            downloads_product = False
            for step in builder['factory'].steps:
                step_name = step.kwargs.get('name', step.step_class.name)
                if step_name == 'configure-build':
                    configure_kwargs = step.kwargs
                if step_name == 'download-built-product':
                    downloads_product = True
            if configure_kwargs is None:
                continue
            arch_for_upload = '-'.join(configure_kwargs['architecture'].split(' '))
            info[builder['name']] = {
                'key': '%s-%s-%s' % (configure_kwargs['platform'], arch_for_upload, configure_kwargs['configuration']),
                'triggers': configure_kwargs.get('triggers') or [],
                'downloads_product': downloads_product,
            }

        for builder_name, data in info.items():
            for trigger_name in data['triggers']:
                for triggered_name in triggerable_builders.get(trigger_name, []):
                    triggered = info.get(triggered_name)
                    if triggered is None or not triggered['downloads_product']:
                        continue
                    self.assertEqual(
                        triggered['key'], data['key'],
                        msg='Builder "%s" uploads its build product to "%s" and triggers "%s", but '
                            '"%s" downloads the build product from "%s". A triggered builder must use '
                            'the same platform-architecture-configuration as the builder that triggers '
                            'it, otherwise it cannot find the uploaded product.'
                            % (builder_name, data['key'], triggered_name, triggered_name, triggered['key']))

    def test_all_builders_are_reachable_by_a_scheduler(self):
        # Every builder must be able to run automatically: either it is attached to an
        # automatic scheduler or it is triggered by other bot.
        cwd = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(cwd, 'config.json')) as config_json:
            raw_config = json.load(config_json)

        root_scheduler_types = ('AnyBranchScheduler', 'Nightly', 'PlatformSpecificScheduler')
        directly_scheduled = set()
        triggerable_builders = {}
        for scheduler in raw_config['schedulers']:
            if scheduler.get('type') in root_scheduler_types:
                directly_scheduled.update(scheduler.get('builderNames', []))
            elif scheduler.get('type') == 'Triggerable':
                triggerable_builders[scheduler['name']] = scheduler.get('builderNames', [])

        builder_triggers = {builder['name']: (builder.get('triggers') or []) for builder in raw_config['builders']}

        # Walk the trigger graph outward from the directly-scheduled builders.
        reachable = set(directly_scheduled)
        frontier = list(directly_scheduled)
        while frontier:
            builder_name = frontier.pop()
            for trigger_name in builder_triggers.get(builder_name, []):
                for triggered_name in triggerable_builders.get(trigger_name, []):
                    if triggered_name not in reachable:
                        reachable.add(triggered_name)
                        frontier.append(triggered_name)

        all_builders = set(builder['name'] for builder in self.config['builders'])
        unreachable = sorted(all_builders - reachable)
        self.assertEqual(
            unreachable, [],
            msg='These builders cannot run automatically: %s. Each is neither attached to a '
                'scheduler (AnyBranchScheduler / Nightly / PlatformSpecificScheduler) nor triggered '
                'by a builder that is. Attach each to a scheduler or trigger it from a build bot.'
                % unreachable)

# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

import loadConfig
import os
import unittest


class TestExpectedBuildSteps(unittest.TestCase):

    expected_steps = {
        'Style-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'fetch-branch-references',
            'show-identifier',
            'update-working-directory',
            'apply-patch',
            'check-webkit-style'
        ],
        'Apply-WatchList-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'fetch-branch-references',
            'show-identifier',
            'update-working-directory',
            'apply-patch',
            'apply-watch-list'
        ],
        'GTK-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'jhbuild',
            'compile-webkit',
            'install-built-product'
        ],
        'GTK-WK2-Tests-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'set-build-summary'
        ],
        'iOS-15-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'iOS-15-Simulator-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'iOS-15-Simulator-WK2-Tests-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'macOS-AppleSilicon-Big-Sur-Debug-Build-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'macOS-AppleSilicon-Big-Sur-Debug-WK2-Tests-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'macOS-Catalina-Release-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'macOS-Catalina-Release-WK1-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'macOS-Catalina-Release-WK2-Tests-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'macOS-Release-WK2-Stress-Tests-EWS': [
            'configure-build',
            'find-modified-layout-tests',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'run-layout-tests-in-stress-mode',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'macOS-Catalina-Debug-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'macOS-Catalina-Debug-WK1-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'wait-for-crash-collection',
            'kill-old-processes',
            'find-modified-layout-tests',
            'run-layout-tests-in-stress-mode',
            'layout-tests',
            'trigger-crash-log-submission',
            'set-build-summary'
        ],
        'watchOS-8-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'watchOS-8-Simulator-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'tvOS-15-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'tvOS-15-Simulator-Build-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'Windows-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit',
            'validate-patch',
            'layout-tests',
            'set-build-summary'
        ],
        'WinCairo-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-webkit'
        ],
        'WPE-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'jhbuild',
            'compile-webkit'
        ],
        'JSC-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-jsc',
            'jscore-test'
        ],
        'JSC-MIPSEL-32bits-Build-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-jsc'
        ],
        'JSC-MIPSEL-32bits-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'jscore-test'
        ],
        'JSC-ARMv7-32bits-Build-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-jsc'
        ],
        'JSC-ARMv7-32bits-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'jscore-test'
        ],
        'JSC-i386-32bits-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'kill-old-processes',
            'compile-jsc'
        ],
        'Bindings-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'bindings-tests'
        ],
        'WebKitPy-Tests-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'webkitpy-tests-python2',
            'webkitpy-tests-python3',
            'set-build-summary'
        ],
        'WebKitPerl-Tests-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'webkitperl-tests'
        ],
        'API-Tests-iOS-Simulator-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'run-api-tests'
        ],
        'API-Tests-macOS-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'run-api-tests'
        ],
        'API-Tests-GTK-EWS': [
            'configure-build',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'jhbuild',
            'download-built-product',
            'extract-built-product',
            'kill-old-processes',
            'run-api-tests'
        ],
        'Services-EWS': [
            'configure-build',
            'check-patch-relevance',
            'validate-patch',
            'configuration',
            'clean-and-update-working-directory',
            'checkout-specific-revision',
            'fetch-branch-references',
            'show-identifier',
            'apply-patch',
            'build-webkit-org-unit-tests',
            'buildbot-check-config-for-build-webkit',
            'ews-unit-tests',
            'buildbot-check-config-for-ews',
            'resultsdbpy-unit-tests'
        ],
        'Commit-Queue': [
            'configure-build',
            'validate-patch',
            'validate-commiter-and-reviewer',
            'configuration',
            'clean-up-git-repo',
            'clean-and-update-working-directory',
            'fetch-branch-references',
            'show-identifier',
            'verify-github-integrity',
            'update-working-directory',
            'apply-patch',
            'validate-changelog-and-reviewer',
            'find-modified-changelogs',
            'kill-old-processes',
            'compile-webkit',
            'kill-old-processes',
            'validate-patch',
            'check-status-on-other-ewses',
            'layout-tests',
            'validate-patch',
            'clean-and-update-working-directory',
            'show-identifier',
            'update-working-directory',
            'apply-patch',
            'create-local-git-commit',
            'push-commit-to-webkit-repo',
            'set-build-summary'
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
                buildSteps.append(step.factory.name)
            self.assertTrue(builder['name'] in self.expected_steps, 'Missing expected steps for builder: %s\n Actual result is %s' % (builder['name'], buildSteps))
            self.assertListEqual(self.expected_steps[builder['name']], buildSteps, msg="Expected steps don't match for builder %s" % builder['name'])

    def test_unnecessary_expected_steps(self):
        builders = set()
        for builder in self.config['builders']:
            builders.add(builder['name'])
        for builder in self.expected_steps:
            self.assertTrue(builder in builders, "Builder %s doesn't exist, but has unnecessary expected steps" % builder)

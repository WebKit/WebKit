# Copyright (C) 2026 Igalia S.L.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import subprocess
import sys
import tempfile
import unittest

GENERATE_DIRTY_FILES = os.path.realpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'generate-dirty-files'))

UNCOUNTED_CALL_ARGS = 'Uncounted call argument for a raw pointer/reference parameter'


def write_report(directory, project, name, bug_file, bug_type=UNCOUNTED_CALL_ARGS):
    project_dir = os.path.join(directory, 'StaticAnalyzer', project)
    os.makedirs(project_dir, exist_ok=True)
    with open(os.path.join(project_dir, name), 'w') as f:
        f.write('<!-- BUGFILE {} -->\n'.format(bug_file))
        f.write('<!-- ISSUEHASHCONTENTOFLINEINCONTEXT {} -->\n'.format(name))
        f.write('<!-- BUGTYPE {} -->\n'.format(bug_type))
        f.write('<!-- BUGLINE 1 -->\n')


def read_files(output_dir, project, checker='UncountedCallArgsChecker'):
    with open(os.path.join(output_dir, project, checker + 'Files')) as f:
        return sorted(line for line in f.read().splitlines() if line)


class GenerateDirtyFilesTest(unittest.TestCase):
    def run_script(self, results_dir, output_dir, build_dir):
        proc = subprocess.run(
            [sys.executable, GENERATE_DIRTY_FILES, results_dir, '--output-dir', output_dir, '--build-dir', build_dir],
            capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        return proc

    def test_xcode_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            results = os.path.join(directory, 'results')
            write_report(results, 'WebCore', 'report-1.html', 'Source/WebCore/dom/Document.cpp')
            write_report(results, 'WebCore', 'report-2.html', 'WebKitBuild/Release/DerivedSources/WebCore/JSDocument.cpp')
            write_report(results, 'WebCore', 'report-3.html', 'WebKitBuild/Release/usr/local/include/wtf/Vector.h')
            output = os.path.join(directory, 'out')
            self.run_script(results, output, os.path.join(directory, 'build'))
            # JS bindings keep their full path so the comparison step can filter them; other projects' headers are ignored.
            self.assertEqual(read_files(output, 'WebCore'), ['DerivedSources/WebCore/JSDocument.cpp', 'dom/Document.cpp'])

    def test_cmake_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            results = os.path.join(directory, 'results')
            build = os.path.join(directory, 'webkit', 'WebKitBuild', 'WPE', 'Release')
            source = os.path.join(directory, 'webkit', 'Source')
            write_report(results, 'WebCore', 'report-1.html', os.path.join(source, 'WebCore/dom/Document.cpp'))
            write_report(results, 'WebCore', 'report-2.html', os.path.join(build, 'WebCore/DerivedSources/JSDocument.cpp'))
            write_report(results, 'WebCore', 'report-3.html', os.path.join(build, 'WebCore/DerivedSources/StyleBuilderGenerated.cpp'))
            write_report(results, 'WebCore', 'report-4.html', os.path.join(build, 'WebCore/PrivateHeaders/WebCore/Document.h'))
            write_report(results, 'WebCore', 'report-5.html', os.path.join(build, 'WTF/Headers/wtf/Vector.h'))
            write_report(results, 'WebKit', 'report-6.html', os.path.join(source, 'ThirdParty/skia/include/core/SkRefCnt.h'))
            write_report(results, 'WebKit', 'report-7.html', os.path.join(source, 'WebKit/UIProcess/WebPageProxy.cpp'))
            write_report(results, 'WebKit', 'report-8.html', os.path.join(build, 'DerivedSources/WebKit/WebProcess/WebPage/WebPageMessageReceiver.cpp'))
            write_report(results, 'WebKit', 'report-9.html', os.path.join(build, 'DerivedSources/ForwardingHeaders/WebKit/Float3.h'))
            write_report(results, 'WebKit', 'report-10.html', os.path.join(source, 'WebKit/Shared/Foo.cpp'), bug_type='Some future checker')
            # CI passes the checkout as --build-dir, so build-tree paths keep their WebKitBuild/<Port>/<Config> prefix.
            for build_dir in (build, os.path.join(directory, 'webkit')):
                output = os.path.join(directory, 'out', os.path.basename(build_dir))
                proc = self.run_script(results, output, build_dir)
                self.assertEqual(read_files(output, 'WebCore'), ['StyleBuilderGenerated.cpp', 'WebCore/DerivedSources/JSDocument.cpp', 'dom/Document.cpp'])
                # The checkout path contains "webkit", which must not attribute every file to the WebKit project.
                self.assertEqual(read_files(output, 'WebKit'), ['UIProcess/WebPageProxy.cpp', 'WebProcess/WebPage/WebPageMessageReceiver.cpp'])
                self.assertIn('Unknown checker for bug type: Some future checker', proc.stderr)


if __name__ == '__main__':
    unittest.main()

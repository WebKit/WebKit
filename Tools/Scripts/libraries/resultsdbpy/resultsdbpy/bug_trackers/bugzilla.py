# Copyright (C) 2022 Apple Inc. All rights reserved.
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

from ast import keyword
from urllib.parse import quote
from resultsdbpy.bug_trackers.bug_description import translate_selected_dots_to_bug_title_and_description
from resultsdbpy.controller.bug_tracker_controller import BugTrackerConfig


class WebKitBugzilla(BugTrackerConfig):

    COMPONENTS_LAYOUT_TEST_MAPPING = {
        'Accessibility': {'lower': True},
        'ANGLE': {'skip': True},
        'Animations': {'lower': True},
        'Bindings': {'lower': True},
        'bmalloc': {'skip': True},
        'Canvas': {'lower': True},
        'CMake': {'skip': True},
        'Compositing': {'lower': True},
        'CSS': {'keywords': [
            'css', 'css1', 'css2.1', 'css3', 'cssom', 'css-custom-properties-api',
            'css-dark-mode', 'css-typedom', 'backgrounds', 'borders', 'box-decoration-break',
            'box-shadow', 'box-sizing', 'flexbox', 'gradients', 'inline', 'inline-block',
            'line-grid', 'multicol', 'overflow', 'transforms']},
        'DOM': {'keywords': ['dom', 'shadow-dom']},
        'Evangelism': {'skip': True},
        'Forms': {'lower': True},
        'Frames': {'lower': True},
        'History': {'lower': True},
        'HTML Editing': {'keywords': ['editing']},
        'Images': {'lower': True},
        'JavaScriptCore': {'keywords': ['js', 'jquery', 'regex']},
        'Layout and Rendering': {'keywords': ['rendering', 'layers']},
        'MathML': {'lower': True},
        'Media': {'keywords': ['media', 'mediacapturefromelement', 'mediasession', 'mediastream']},
        'New Bugs': {'keywords': ['imported']},
        'Page Loading': {'skip': True},
        'PDF': {'skip': True},
        'Platform': {'skip': True},
        'Plug-ins': {'skip': True},
        'Printing': {'lower': True},
        'Scrolling': {'keywords': ['scrolling', 'fast-moblie-scrolling']},
        'Service Workers': {'keywords': ['workers']},
        'SVG': {'lower': True},
        'Tables': {'lower': True},
        'Text': {'lower': True},
        'Tools / Tests': {'skip': True},
        'UI Events': {'keywords': ['events', 'eventloop']},
        'WebAssembly': {'keywords': ['wasm']},
        'Web Audio': {'keywords': ['webaudio']},
        'WebCore JavaScript': {'skip': True},  # FIXME maybe need move some keywords from JavaScriptCore
        'WebCore Misc.': {'keywords': ['misc']},
        'WebDriver': {'skip': True},
        'WebGL': {'lower': True},
        'WebGPU': {'keywords': ['gpu-process']},
        'Web Inspector': {'keywords': ['inspector']},
        'WebKit2': {'skip': True},
        'WebKit API': {'skip': True},
        'WebKitGTK': {'skip': True},
        'WebKit Misc.': {'keywords': ['misc']},
        'WebKit Process Model': {'skip': True},
        'WebKit Website': {'skip': True},
        'WebRTC': {'lower': True},
        'Website Storage': {'keywords': ['storage']},
        'Web Template Framework'
        'WebXR': {'lower': True},
        'WPE WebKit': {'skip': True},
        'XML': {'lower': True},
    }

    def __init__(self):
        super().__init__('bugzilla')

    def create_bug(self, content):
        """
            content formats:
            {
                "selectedRows": [{
                    "config": {
                        "model":  ...,
                        "architecture": ...,
                        "style": ...,
                        "flavor": ...,
                        "sdk": ...,
                        "version_name": ...,
                        "suite": ...
                    },
                    "results": [
                        {
                            "uuid": ...,
                            "actual": ...,
                            "expected": ...,
                            "start_time": ...,
                            "time": ...
                        }
                    ]
                }],
                "willFilterExpected": ...,
                "repositories": [],
                "suite": ...,
                "test": ...
            }
        """

        selected_rows = content['selectedRows']
        will_filter_expected = content['willFilterExpected']
        repositories = content['repositories']
        suite = content['suite']
        test = content['test']

        title_components, description_components = translate_selected_dots_to_bug_title_and_description(self.commit_context, selected_rows, test, suite, repositories, will_filter_expected)

        component = 'New Bugs'
        version = 'WebKit Nightly Build'
        if suite == 'api-tests':
            component = 'WebKit API'
        if suite == 'webkitpy-tests':
            component = 'Tools / Tests'
        if suite == 'internal-media-tests':
            component = 'Media'
        if suite == 'javascriptcore-tests':
            component = 'JavaScriptCore'
        if test and any([suite == 'layout-tests', suite == 'internal-security-tests']):
            test_name_parts = test.split('/')
            for k, v in self.COMPONENTS_LAYOUT_TEST_MAPPING.items():
                if 'skip' in v and v['skip']:
                    continue
                if 'lower' in v and v['lower']:
                    if k.lower() in test_name_parts:
                        component = k
                        break
                if 'keywords' in v and len(v['keywords']):
                    if any(map(lambda keyword: keyword in test_name_parts, v['keywords'])):
                        component = k
                        break

        bugzilla_url_components = [
            'component={}'.format(quote(component)),
            'version={}'.format(quote(version)),
            'short_desc={} {}'.format(
                quote(test if test else suite),
                quote(' and '.join(list(title_components)))
            ),
            'comment={}'.format(
                quote('\n'.join(list(description_components)))
            )
        ]

        return {
            'url': 'https://bugs.webkit.org/enter_bug.cgi?product=WebKit&{}'.format('&'.join(bugzilla_url_components)),
            'newWindow': True
        }

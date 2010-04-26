#
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'variables': {
        # List of DevTools source files, ordered by dependencies. It is used both
        # for copying them to resource dir, and for generating 'devtools.html' file.
        'devtools_js_files': [
            'src/js/InspectorControllerImpl.js',
            'src/js/DebuggerAgent.js',
            'src/js/DebuggerScript.js',
            'src/js/ProfilerAgent.js',
            'src/js/ProfilerProcessor.js',
            'src/js/HeapProfilerPanel.js',
            'src/js/DevTools.js',
            'src/js/DevToolsHostStub.js',
            'src/js/Tests.js',
        ],
        'devtools_css_files': [
            'src/js/devTools.css',
        ],
        'devtools_image_files': [
            'src/js/Images/segmentChromium.png',
            'src/js/Images/segmentHoverChromium.png',
            'src/js/Images/segmentHoverEndChromium.png',
            'src/js/Images/segmentSelectedChromium.png',
            'src/js/Images/segmentSelectedEndChromium.png',
            'src/js/Images/statusbarBackgroundChromium.png',
            'src/js/Images/statusbarBottomBackgroundChromium.png',
            'src/js/Images/statusbarButtonsChromium.png',
            'src/js/Images/statusbarMenuButtonChromium.png',
            'src/js/Images/statusbarMenuButtonSelectedChromium.png',
        ],
    },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

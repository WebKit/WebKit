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

import sys

if sys.version_info > (3, 0):
    import html5lib
    import bs4
    from bs4.builder import builder_registry, TreeBuilder
    from bs4.builder._htmlparser import HTMLParserTreeBuilder

    class BeautifulSoup(bs4.BeautifulSoup):
        HTML_ENTITIES = 'html'

        def __init__(self, *args, **kwargs):
            if 'convertEntities' in kwargs:
                del kwargs['convertEntities']
            if 'parseOnlyThese' in kwargs:
                kwargs['parse_only'] = kwargs['parseOnlyThese']
                del kwargs['parseOnlyThese']
            kwargs['features'] = self.HTML_ENTITIES
            kwargs['builder'] = HTMLParserTreeBuilder
            super(BeautifulSoup, self).__init__(*args, **kwargs)

    class BeautifulStoneSoup(bs4.BeautifulSoup):
        XML_ENTITIES = 'xml'

        def __init__(self, *args, **kwargs):
            if 'convertEntities' in kwargs:
                del kwargs['convertEntities']
            kwargs['features'] = self.XML_ENTITIES

            # FIXME: This hack isn't strictly accurate, but getting lxml to work with the autoinstaller is a non-trivial task
            kwargs['builder'] = HTMLParserTreeBuilder
            super(BeautifulStoneSoup, self).__init__(*args, **kwargs)

    SoupStrainer = bs4.SoupStrainer
else:
    from ews.thirdparty.BeautifulSoup_legacy import BeautifulSoup, SoupStrainer

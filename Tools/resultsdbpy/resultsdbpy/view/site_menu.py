# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from collections import OrderedDict
from flask import request, url_for


class SiteMenu(object):

    def __init__(self, title):
        self.title = title
        self._name_link_mapping = OrderedDict()

    def add_link(self, name, url):
        self._name_link_mapping[name] = lambda: url

    def add_endpoint(self, name, endpoint):
        self._name_link_mapping[name] = lambda: url_for(endpoint)

    @classmethod
    def render_with_site_menu(cls):
        def decorator(method):
            def real_method(obj, **kwargs):
                site_menu = getattr(obj, 'site_menu', None)
                if not site_menu:
                    return method(obj, site_title='?', site_menu={}, **kwargs)
                return method(obj, site_title=site_menu.title, site_menu=site_menu._name_link_mapping, request=request, **kwargs)

            real_method.__name__ = method.__name__
            return real_method

        return decorator

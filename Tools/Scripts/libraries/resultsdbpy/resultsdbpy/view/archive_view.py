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

from flask import abort, jsonify, redirect, request, Response
from resultsdbpy.controller.commit_controller import uuid_range_for_query, HasCommitContext
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from resultsdbpy.view.site_menu import SiteMenu
from urllib.parse import urlparse
from webkitflaskpy.util import AssertRequest, boolean_query, cache_for, limit_for_query, query_as_kwargs, query_as_string


class ArchiveView(HasCommitContext):
    DEFAULT_LIMIT = 10
    FORMAT = dict(
        json='application/json',
        css='text/css',
        html='text/html',
        txt='text/plain',
        png='image/png',
        jpeg='image/jpeg',
    )

    def __init__(self, environment, archive_controller, site_menu=None):
        super(ArchiveView, self).__init__(archive_controller.commit_context)
        self.environment = environment

        self.archive_context = archive_controller.archive_context
        self.upload_context = archive_controller.upload_context
        self.site_menu = site_menu

    @SiteMenu.render_with_site_menu()
    def list(self, path, values, **kwargs):
        return self.environment.get_template('archive_list.html').render(
            title=f'{self.site_menu.title}: archive/f{path or ""}',
            path=path or '', values=values,
            query=query_as_string(),
            **kwargs)

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    @cache_for(hours=12)
    def extract(
        self, path=None, format=None,
        suite=None, configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        # When a file (not a listing) is requested with no narrowing query,
        # fall back to the parent page's query via the Referer header.
        # Result-file links inside served results.html (e.g. *-diff.txt)
        # are sometimes built by JS templates that we don't rewrite, so
        # they navigate without our query and would otherwise match every
        # recent archive across configs. Listings are deliberately
        # exempted so cross-page navigation never silently narrows them.
        is_listing = not path or path.endswith('/')
        if not is_listing and not request.query_string and request.referrer:
            referrer = urlparse(request.referrer)
            if referrer.netloc == request.host and referrer.query:
                return redirect(f'{request.path}?{referrer.query}')

        recent = boolean_query(*recent)[0] if recent else True

        if not suite:
            suites = set()
            for config_suites in self.upload_context.find_suites(configurations=configurations, recent=recent).values():
                [suites.add(suite) for suite in config_suites]
        else:
            suites = set(suite)

        result = None
        with self.archive_context, self.upload_context:
            for suite in suites:
                try:
                    matches = self.archive_context.file(
                        path=path,
                        configurations=configurations, suite=suite, branch=branch[0],
                        begin=begin, end=end, recent=recent, limit=2,
                        begin_query_time=begin_query_time, end_query_time=end_query_time,
                    )
                except RuntimeError as error:
                    abort(400, f'Query too broad to fetch archives: {error}. Narrow by uuid or configuration.')
                for files in matches.values():
                    for file in files:
                        candidate = file.get('file')
                        if not candidate:
                            continue
                        if not result:
                            if isinstance(candidate, list):
                                result = set(candidate)
                            else:
                                result = candidate
                            continue
                        if isinstance(candidate, list) and isinstance(result, set):
                            result |= set(candidate)
                            continue
                        if result == candidate:
                            continue
                        abort(403, 'Multiple archives match with different content')

        if not result:
            abort(404, f"No archive content{' at ' + path if path else ''}")

        if isinstance(result, set):
            return self.list(path=path, values=sorted(result))

        mimetype = self.FORMAT.get(format)
        if not mimetype and '.' in path:
            mimetype = self.FORMAT.get(path.split('.')[-1])
        mimetype = mimetype or 'text/plain'

        # Patch link templates in served results.html to carry our query
        # string. Each pattern is a JS string concatenation we expect to
        # find verbatim; results.html has been rewritten more than once,
        # so we handle every variant we still ship and the variants that
        # exist in older archives we still serve.
        if mimetype == 'text/html':
            query = query_as_string()
            link_rewrites = (
                (
                    b"href=\"' + encodeURI(path + suffix) + '\"",
                    f"href=\"' + encodeURI(path + suffix) + '{query}' + '\"".encode('utf-8'),
                ),
                (
                    b"href=\"' + testPrefix + suffix + '\"",
                    f"href=\"' + testPrefix + suffix + '{query}' + '\"".encode('utf-8'),
                ),
            )
            for old, new in link_rewrites:
                result = result.replace(old, new)

            for include_to_strip in ['js/status-bubble.js', 'code-review.js?version=48']:
                result = result.replace(f'<script src="{include_to_strip}"></script>'.encode('utf-8'), ''.encode('utf-8'))

            if (query):
                result = result.replace("src += '?format=txt'".encode('utf-8'), "src += '&format=txt'".encode('utf-8'))

        return Response(result, mimetype=mimetype or 'text/plain')

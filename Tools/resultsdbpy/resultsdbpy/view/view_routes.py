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

import os
import requests
import time

from flask import abort, send_from_directory, redirect, Response
from jinja2 import Environment, PackageLoader, select_autoescape
from resultsdbpy.flask_support.util import AssertRequest
from resultsdbpy.flask_support.authed_blueprint import AuthedBlueprint
from resultsdbpy.view.ci_view import CIView
from resultsdbpy.view.commit_view import CommitView
from resultsdbpy.view.site_menu import SiteMenu
from resultsdbpy.view.suite_view import SuiteView
from werkzeug.exceptions import HTTPException, InternalServerError


class ViewRoutes(AuthedBlueprint):
    def __init__(self, model, controller, import_name=__name__, title='Results Database', auth_decorator=None):
        super(ViewRoutes, self).__init__('view', import_name, url_prefix=None, auth_decorator=auth_decorator)
        self._cache = {}

        self.title = title
        self.environment = Environment(
            loader=PackageLoader(package_name='resultsdbpy.view', package_path='templates'),
            autoescape=select_autoescape(['html', 'xml']),
        )

        # Protecting js and css with auth doesn't make sense
        self.add_url_rule('/library/<path:path>', 'library', self.library, authed=False, methods=('GET',))
        self.add_url_rule('/assets/<path:path>', 'assets', self.assets, authed=False, methods=('GET',))

        self.site_menu = SiteMenu(title=self.title)

        self.commits = CommitView(
            environment=self.environment,
            commit_controller=controller.commit_controller,
            site_menu=self.site_menu,
        )
        self.suites = SuiteView(
            environment=self.environment,
            upload_controller=controller.upload_controller,
            suite_controller=controller.suite_controller,
            site_menu=self.site_menu,
        )
        self.ci = CIView(
            environment=self.environment,
            ci_controller=controller.ci_controller,
            site_menu=self.site_menu,
        )

        self.add_url_rule('/', 'main', self.suites.search, methods=('GET',))
        self.add_url_rule('/search', 'search', self.suites.search, methods=('GET',))

        self.add_url_rule('/documentation', 'documentation', self.documentation, methods=('GET',))

        self.add_url_rule('/commit', 'commit', self.commits.commit, methods=('GET',))
        self.add_url_rule('/commit/info', 'commit_info', self.commits.info, methods=('GET',))
        self.add_url_rule('/commit/previous', 'commit_previous', self.commits.previous, methods=('GET',))
        self.add_url_rule('/commit/next', 'commit_next', self.commits.next, methods=('GET',))
        self.add_url_rule('/commits', 'commits', self.commits.commits, methods=('GET',))
        self.add_url_rule('/suites', 'suites', self.suites.results, methods=('GET',))

        self.add_url_rule('/urls/queue', 'urls-queue', self.ci.queue, methods=('GET',))
        self.add_url_rule('/urls/worker', 'urls-worker', self.ci.worker, methods=('GET',))
        self.add_url_rule('/urls/build', 'urls-build', self.ci.build, methods=('GET',))

        self.site_menu.add_endpoint('Main', self.name + '.main')
        self.site_menu.add_endpoint('Suites', self.name + '.suites')
        self.site_menu.add_endpoint('Documentation', self.name + '.documentation')
        self.site_menu.add_endpoint('Commits', self.name + '.commits')

        self.register_error_handler(500, self.response_500)

    def assets(self, path):
        AssertRequest.no_query()
        AssertRequest.is_type()
        path_split = os.path.split(path)
        return send_from_directory(os.path.join(os.path.abspath(os.path.dirname(__file__)), 'static', *path_split[:-1]), path_split[-1])

    def cache_resource(self, url):
        # This avoids headaches with cross-origin scripts
        cached_result = self._cache.get(url, None)
        if cached_result and cached_result[0] + 60 * 60 * 24 > time.time():
            return cached_result[1]
        result = requests.get(url)
        if result.status_code != 200:
            abort(404, description=f'Failed to cache resource at {url}')
        self._cache[url] = (time.time(), Response(result.text, mimetype=result.headers['content-type']))
        return self._cache[url][1]

    def library(self, path):
        AssertRequest.no_query()
        AssertRequest.is_type()
        path_split = os.path.split(path)
        return send_from_directory(os.path.join(os.path.abspath(os.path.dirname(__file__)), 'static/library', *path_split[:-1]), path_split[-1])

    def response_500(self, error=InternalServerError()):
        if isinstance(error, HTTPException):
            return self.error(error=error)
        return self.error(error=InternalServerError())

    @SiteMenu.render_with_site_menu()
    def error(self, error=InternalServerError(), **kwargs):
        response = self.environment.get_template('error.html').render(
            title=f'{self.title}: {error.code}',
            name=error.name, description=error.description,
            **kwargs)
        return response

    @SiteMenu.render_with_site_menu()
    def documentation(self, **kwargs):
        return self.environment.get_template('documentation.html').render(
            title=f'{self.title}: Documentation',
            **kwargs)

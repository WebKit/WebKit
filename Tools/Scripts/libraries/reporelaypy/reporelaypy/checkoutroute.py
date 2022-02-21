# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import json
import re

from flask import abort, current_app, json as fjson, redirect, Flask, Response
from reporelaypy import Database
from webkitflaskpy import AuthedBlueprint
from webkitscmpy import Commit, remote


class Redirector(object):
    class Encoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, Redirector):
                return super(Checkout.Encoder, self).default(obj)

            return dict(
                name=obj.name,
                url=obj.url,
            )

    @classmethod
    def bitbucket_generator(cls, base):
        def redirector(commit):
            if commit and commit.hash:
                return redirect('{}/commits/{}'.format(base, commit.hash))
            return redirect('{}/commits'.format(base))

        return redirector

    @classmethod
    def trac_generator(cls, base):
        def redirector(commit):
            if commit and commit.revision:
                return redirect('{}/changeset/{}/webkit'.format(base, commit.revision))
            return redirect(base)

        return redirector

    @classmethod
    def github_generator(cls, base):
        def redirector(commit):
            if commit and commit.hash:
                return redirect('{}/commit/{}'.format(base, commit.hash))
            return redirect('{}/commits'.format(base))

        return redirector

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        if isinstance(data, list):
            return [cls(**node) for node in data]
        return cls(**data)

    def __init__(self, url, name=None):
        self.url = url
        self.type = None
        self._redirect = None

        for key, params in dict(
            bitbucket=(remote.BitBucket.URL_RE, self.bitbucket_generator),
            trac=(re.compile(r'\Ahttps?://trac.(?P<domain>\S+)\Z'), self.trac_generator),
            github=(remote.GitHub.URL_RE, self.github_generator),
        ).items():
            regex, generator = params
            if regex.match(url):
                self.type = key
                self._redirect = generator(url)
                break
        if not self.type:
            raise TypeError("'{}' is not a recognized redirect base")
        self.name = name or self.type

    def __call__(self, commit):
        if not self._redirect:
            abort(Response("No valid redirect for '{}'".format(self.url), status=500))
        return self._redirect(commit)


class CheckoutRoute(AuthedBlueprint):
    def __init__(self, checkout, redirectors=None, import_name=__name__, auth_decorator=None, database=None):
        super(CheckoutRoute, self).__init__('checkout', import_name, url_prefix=None, auth_decorator=auth_decorator)

        self.checkout = checkout
        self.database = database or Database()

        if not redirectors:
            redirectors = [Redirector(self.checkout.url)]
        self.redirectors = redirectors

        self.add_url_rule('/', 'landing', lambda: self.redirectors[0](None), methods=('GET',))
        self.add_url_rule(
            '/<path:ref>', 'redirect',
            lambda ref: self.redirectors[0](self.commit(ref)),
            methods=('GET',),
        )
        for redirector in self.redirectors:
            self.add_url_rule(
                '/{}'.format(redirector.name), 'landing-{}'.format(redirector.name),
                lambda redirector=redirector: redirector(None),
                methods=('GET',),
            )
            self.add_url_rule(
                '/<path:ref>/{}'.format(redirector.name), 'redirect-{}'.format(redirector.name),
                lambda ref, redirector=redirector: redirector(self.commit(ref)), methods=('GET',),
            )

        self.add_url_rule('/<path:ref>/json', 'api', self.api, methods=('GET',))
        self.add_url_rule('/changeset/<path:revision>/webkit', 'trac', self.trac, methods=('GET',))

    def commit(self, ref=None):
        try:
            retrieved = self.database.get(ref)
            if retrieved:
                return Commit.from_json(retrieved)
        except (redis.exceptions.ResponseError, TypeError, ValueError) as e:
            sys.stderr.write('{}\n'.format(e))

        if not self.checkout.repository:
            return None

        commit = None
        try:
            commit = self.checkout.repository.find(ref) if ref else self.checkout.repository.commit()
        except (RuntimeError, ValueError):
            pass

        if not commit:
            try:
                if self.checkout.fallback_repository:
                    return self.checkout.fallback_repository.find(ref)
            except (RuntimeError, ValueError):
                pass
            return None

        encoded = json.dumps(commit, cls=Commit.Encoder)
        self.database.set(commit.hash, encoded)
        self.database.set('r{}'.format(commit.revision), encoded)
        self.database.set(str(commit), encoded)

        return commit

    def api(self, ref):
        try:
            return current_app.response_class(
                fjson.dumps(Commit.Encoder().default(self.commit(ref)), indent=4),
                mimetype='application/json',
            )

        except BaseException as e:
            return current_app.response_class(
                fjson.dumps(dict(
                    status='Not Found',
                    message='No commit with the specified reference could be found',
                    error=str(e),
                ), indent=4),
                mimetype='application/json',
                status=404,
            )

    def trac(self, revision):
        try:
            return self.redirectors[0](self.commit('r{}'.format(int(revision))))
        except BaseException as e:
            return current_app.response_class(
                fjson.dumps(dict(
                    status='Server Error',
                    message='Failed to digest trac link',
                    error=str(e),
                ), indent=4),
                mimetype='application/json',
                status=500,
            )

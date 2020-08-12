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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS `"AS IS" AND
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
from flask import abort, request


class FlaskJSONEncoder(json.JSONEncoder):
    # Flask's jsonify only accepts a dictionary and does not accept a JSON encoder. When overriding the default JSON
    # encoder, encoder.default(...) will return a dictionary to be serialized. However, the default encoder will raise
    # an exception if default(...) is called on primative JSON types. Implement a version of the default encoder which
    # passes primative JSON types back to the caller.
    def default(self, obj):
        if isinstance(obj, dict):
            return {key: self.default(value) for key, value in obj.items()}
        if isinstance(obj, list):
            return [self.default(value) for value in obj]
        return obj


class AssertRequest(object):
    @classmethod
    def is_type(cls, supported_requests=None):
        if supported_requests is None:
            supported_requests = ['GET']
        if not supported_requests:
            abort(500, description='Endpoint does not support any requests')
        if request.method not in supported_requests:
            abort(405, description='Endpoint only supports {} requests'.format(
                supported_requests[0] if len(supported_requests) == 1 else ', '.join(supported_requests[:-1]) + ' and ' + supported_requests[-1],
            ))

    @classmethod
    def no_query(cls):
        if request.query_string:
            abort(400, description='Queries not supported on this endpoint')

    @classmethod
    def query_kwargs_empty(cls, **kwargs):
        for key, value in kwargs.items():
            if value:
                abort(400, description=f"'{key}' not supported in queries by this endpoint")


def query_as_kwargs():
    def decorator(method):
        def real_method(val, method=method, **kwargs):
            for key, value in request.args.to_dict(flat=False).items():
                if key in kwargs:
                    abort(400, description=f'{key} is not a valid query parameter on this endpoint')
                kwargs[key] = tuple(value)
            return method(val, **kwargs)

        real_method.__name__ = method.__name__
        return real_method
    return decorator


def query_as_string():
    query = '?'
    for key, values in request.args.to_dict(flat=False).items():
        for value in values:
            query += f'{key}={value}&'
    return query[:-1]


def boolean_query(*args):

    def func(string):
        if string.lower() in ['true', 'yes']:
            return True
        try:
            return bool(int(string))
        except ValueError:
            return False

    return [func(arg) for arg in args]


def limit_for_query(default_limit=100):
    def decorator(method):
        def real_method(self=None, method=method, limit=None, **kwargs):
            limit_to_use = default_limit
            if limit:
                try:
                    limit_to_use = int(limit[-1])
                    if limit_to_use <= 0:
                        raise ValueError()
                except ValueError:
                    abort(400, description='Limit must be a positive integer')
            if self:
                return method(self=self, limit=limit_to_use, **kwargs)
            return method(limit=limit_to_use, **kwargs)

        real_method.__name__ = method.__name__
        return real_method
    return decorator


def cache_for(hours=12):
    def decorator(method):
        def real_method(self=None, method=method, **kwargs):
            if self:
                response = method(self=self, **kwargs)
            else:
                response = method(**kwargs)
            response.headers.add('Cache-Control', f'public,max-age={hours * 60 * 60}')
            return response

        real_method.__name__ = method.__name__
        return real_method
    return decorator

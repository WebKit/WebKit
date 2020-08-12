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

from resultsdbpy.flask_support.util import boolean_query
from resultsdbpy.controller.configuration import Configuration


def configuration_for_query():
    def decorator(method):
        def real_method(
                self=None, platform=None, version=None, sdk=None, version_name=None, is_simulator=None,
                architecture=None, model=None, style=None, flavor=None, **kwargs):
            args = dict(
                platform=platform or [], version=version or [], sdk=sdk or [], version_name=version_name or [],
                is_simulator=boolean_query(*(is_simulator or [])), architecture=architecture or [], model=model or [],
                style=style or [], flavor=flavor or [],
            )

            def recursive_callback(keys, **kwargs):
                if not keys:
                    return {Configuration(**kwargs)}

                if not args[keys[-1]]:
                    return recursive_callback(keys[:-1], **kwargs)
                result = set()
                for element in args[keys[-1]]:
                    forwarded_args = {key: value for key, value in kwargs.items()}
                    forwarded_args[keys[-1]] = element
                    result = result.union(recursive_callback(keys[:-1], **forwarded_args))
                return result

            if self is None:
                return method(configurations=list(recursive_callback(list(args.keys()))) or [Configuration()], **kwargs)
            return method(self, configurations=list(recursive_callback(list(args.keys()))) or [Configuration()], **kwargs)

        real_method.__name__ = method.__name__
        return real_method

    return decorator

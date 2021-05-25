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

import unittest

try:
    import pytest
except ImportError:
    pass


def xfail(*args, **kwargs):
    """a pytest.mark.xfail-like wrapper for unittest.expectedFailure

    c.f. pytest.mark.xfail(condition, ..., *, reason=..., run=True, raises=None, strict=[from config])

    Note that this doesn't support raises or strict"""

    # shortcut if we're being called as a decorator ourselves
    if len(args) == 1 and callable(args[0]) and not kwargs:
        return unittest.expectedFailure(args[0])

    def decorator(func):
        conditions = args
        reason = kwargs.get(reason, None)
        run = kwargs.get(run, True)
        if "raises" in kwargs:
            raise TypeError("xfail(raises=...) is not supported")
        if "strict" in kwargs:
            raise TypeError("strict is not supported")

        if not run:
            return unittest.skip(reason)(func)

        if all(conditions):
            return unittest.expectedFailure(func)
        else:
            return func

    return decorator


def slow(func):
    try:
        return pytest.mark.slow(func)
    except NameError:
        return func


skip = unittest.skip

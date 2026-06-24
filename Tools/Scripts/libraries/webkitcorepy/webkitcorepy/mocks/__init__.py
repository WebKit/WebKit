# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import importlib

from webkitcorepy.call_by_need import CallByNeed

# ContextStack is a base class for OutputCapture, OutputDuplicate, and PartialProxy, all
# of which webkitcorepy/__init__.py loads eagerly, so it's effectively never lazy.
from webkitcorepy.mocks.context_stack import ContextStack


def _lazy(module_path, attr_name):
    return CallByNeed(lambda: getattr(importlib.import_module(module_path), attr_name))


# Other symbols load lazily — see __getattr__ — so callers importing only ContextStack
# don't pay for time_/subprocess/requests_/etc.
_LAZY = {
    'Time': _lazy('webkitcorepy.mocks.time_', 'Time'),
    'ProcessCompletion': _lazy('webkitcorepy.mocks.subprocess', 'ProcessCompletion'),
    'Subprocess': _lazy('webkitcorepy.mocks.subprocess', 'Subprocess'),
    'Response': _lazy('webkitcorepy.mocks.requests_', 'Response'),
    'Requests': _lazy('webkitcorepy.mocks.requests_', 'Requests'),
    'Terminal': _lazy('webkitcorepy.mocks.terminal', 'Terminal'),
    'FileLock': _lazy('webkitcorepy.mocks.file_lock', 'FileLock'),
    'Environment': _lazy('webkitcorepy.mocks.environment', 'Environment'),
}


def __getattr__(name):
    proxy = _LAZY.get(name)
    if proxy is None:
        raise AttributeError("module 'webkitcorepy.mocks' has no attribute {!r}".format(name))
    # Resolve through CallByNeed and store the real object in globals(), so future
    # accesses skip __getattr__ and so pickle/isinstance see the underlying class
    # rather than the proxy.
    value = proxy.value
    globals()[name] = value
    return value


def __dir__():
    return sorted(set(globals()) | set(_LAZY))

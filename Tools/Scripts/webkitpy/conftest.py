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

import sys
import types
import warnings

import pytest
from webkitcorepy import AutoInstall

from webkitpy.test import markers


def pytest_configure(config):
    markers._running_under_pytest = True
    config.addinivalue_line("markers", "serial: tests that must be run in serial")
    config.addinivalue_line("markers", "integration: integration tests")
    config.addinivalue_line("markers", "slow: tests that take a while to run")


def pytest_unconfigure(config):
    markers._running_under_pytest = False


def pytest_addoption(parser):
    parser.addoption(
        "--run-slow", action="store_true", default=False, help="run slow tests"
    )


@pytest.hookimpl(tryfirst=True)
def pytest_pycollect_makeitem(collector, name, obj):
    try:
        ut = sys.modules["unittest"]
        if not issubclass(obj, ut.TestCase):
            return None
    except Exception:
        return None

    if getattr(obj, "__pytest_no_rewrite__", False):
        return None

    for attr_name in set(dir(obj)):
        serial = False
        integration = False
        if attr_name.startswith("serial_integration_test_"):
            serial = True
            integration = True
        elif attr_name.startswith("serial_test_"):
            serial = True
        elif attr_name.startswith("integration_test_"):
            integration = True
        else:
            continue

        method = getattr(obj, attr_name)
        if not callable(method):
            continue

        new_attr_name = "test_" + attr_name

        existing_attr = getattr(obj, new_attr_name, None)
        if existing_attr:
            if method != existing_attr:
                warnings.warn(
                    "attribute %r already defined on %r; %r might hide %r"
                    % (new_attr_name, obj, method, existing_attr)
                )

        if sys.version_info < (3,) and isinstance(method, types.MethodType):
            method = method.im_func

        if serial:
            method = pytest.mark.serial(method)

        if integration:
            method = pytest.mark.integration(method)

        setattr(obj, new_attr_name, method)

    return None


def pytest_collection_modifyitems(config, items):
    if hasattr(config, "workerinput"):
        skip_serial = pytest.mark.skip(reason="cannot run in parallel")
        for item in items:
            if "serial" in item.keywords:
                item.add_marker(skip_serial)

    if not config.getoption("--run-slow"):
        skip_slow = pytest.mark.skip(reason="need --run-slow option to run")
        for item in items:
            if "slow" in item.keywords:
                item.add_marker(skip_slow)


def pytest_collection_finish(session):
    AutoInstall.install_everything()

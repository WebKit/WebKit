# Copyright (C) 2026 Apple Inc. All rights reserved.
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


import optparse
import re
from collections import namedtuple

from webkitcorepy import Version

from webkitpy.api_tests.test_expectations import APITestExpectations
from webkitpy.common.host import Host
from webkitpy.common.version_name_map import INTERNAL_TABLE, VersionNameMap
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.port.base import Port
from webkitpy.xcode.device_type import DeviceType

LAYOUT_TESTS = 'layout-tests'
API_TESTS = 'api-tests'

FLAVOR_OPTIONS = {
    LAYOUT_TESTS: {
        None: {},
        'wk1': {'driver_names': ('DumpRenderTree',)},
        'wk2': {},
        'accessibility-isolated-tree': {'accessibility_isolated_tree': True, 'result_report_flavor': 'accessibility-isolated-tree'},
        'site-isolation': {'site_isolation_enabled_by_default': True},
    },
    API_TESTS: {
        None: {},
        'site-isolation': {'site_isolation_enabled_by_default': True},
    },
}

EMBEDDED_PORT_NAMES = {
    ('ios', True): 'ios-simulator',
    ('ios', False): 'ios-device',
    ('visionos', True): 'visionos-simulator',
}
IOS_SIMULATOR_PORT_NAMES = {
    'iPhone': 'iphone-simulator',
    'iPad': 'ipad-simulator',
}

VERSION_OFFSET = 1000

BUG_RE = re.compile(r'webkit\.org/b/\d+|rdar://(?:problem/)?\d+')

ExpectationLine = namedtuple('ExpectationLine', ['filename', 'line_number', 'bugs', 'expectations'])


def _version(configuration):
    version = configuration.get('version')
    if isinstance(version, int):
        return Version(version // VERSION_OFFSET ** 2, version // VERSION_OFFSET % VERSION_OFFSET, version % VERSION_OFFSET)
    if not version:
        raise ValueError('{} has no version'.format(configuration))
    return Version.from_string(version)


def _port_name(host, configuration):
    platform = configuration.get('platform')
    if platform == 'mac':
        version = _version(configuration)
        version_names = VersionNameMap.map(host.platform)
        name = version_names.to_name(version, platform='mac') or version_names.to_name(version, platform='mac', table=INTERNAL_TABLE)
        if not name:
            raise ValueError('webkitpy has no name for macOS {}'.format(version))
        return 'mac-{}'.format(name.lower().replace(' ', ''))

    port_name = EMBEDDED_PORT_NAMES.get((platform, bool(configuration.get('is_simulator'))))
    if not port_name:
        raise ValueError('webkitpy has no port for {}'.format(configuration))
    if port_name == 'ios-simulator' and configuration.get('model'):
        return IOS_SIMULATOR_PORT_NAMES.get(DeviceType.from_string(configuration['model']).hardware_family, port_name)
    return port_name


def _options(configuration, suite):
    if suite not in FLAVOR_OPTIONS:
        raise ValueError('webkitpy cannot map {} configurations'.format(suite))
    flavor = configuration.get('flavor')
    if flavor not in FLAVOR_OPTIONS[suite]:
        raise ValueError("webkitpy has no options for the '{}' flavor of {}".format(flavor, suite))
    style = configuration.get('style')
    if style not in Port.ALL_BUILD_TYPES:
        raise ValueError("webkitpy has no '{}' style".format(style))

    values = dict(
        configuration='Debug' if style == 'debug' else 'Release',
        architecture=configuration.get('architecture'),
        guard_malloc=style == 'guard-malloc',
        asan=style == 'asan',
        **FLAVOR_OPTIONS[suite][flavor]
    )
    if configuration.get('platform') != 'mac':
        values['version'] = str(_version(configuration))

    if suite == API_TESTS:
        return optparse.Values(values)
    options, _ = run_webkit_tests.parse_args([])
    for name, value in values.items():
        setattr(options, name, value)
    return options


def port_for_configuration(configuration, suite, host=None):
    """The port and device type that ran suite on a Results DB configuration; the inverse of Port.configuration_for_upload."""
    host = host or Host()
    options = _options(configuration, suite)
    port = host.port_factory.get(_port_name(host, configuration), options)
    if suite == LAYOUT_TESTS:
        run_webkit_tests.set_up_flavor_options(port, options, checkout_root=port.path_from_webkit_base())
    # A local ASan build would otherwise change which lines apply.
    port._test_configuration = TestConfiguration(port.version_name(), port.architecture(), configuration['style'])

    model = configuration.get('model')
    device_type = DeviceType.from_string(model) if model and configuration.get('platform') != 'mac' else None
    return port, device_type


def _layout_test_expectation_lines(port, device_type, tests):
    expectations = TestExpectations(port, tests=tests, device_type=device_type)
    expectations.parse_all_expectations()
    model = expectations.model()
    lines = {}
    for test in tests:
        line = model.get_expectation_line(test)
        if not line or not line.filename:
            continue
        expected = ('SKIP',) if 'SKIP' in line.modifiers else tuple(line.expectations)
        lines[test] = ExpectationLine(line.filename, line.line_number, tuple(BUG_RE.findall(line.original_string)), expected)
    return lines


def _api_test_expectation_lines(port, tests):
    expectations = APITestExpectations(port, tests=tests)
    expectations.parse_all_expectations()
    contents = {}
    lines = {}
    for test in tests:
        expectation = expectations.get_expectation(test)
        if not expectation:
            continue
        if expectation.filename not in contents:
            contents[expectation.filename] = port.host.filesystem.read_text_file(expectation.filename).splitlines()
        text = contents[expectation.filename][expectation.line_number - 1]
        expected = ('SKIP',) if expectation.is_skip() else tuple(status.name for status in type(expectation.expected) if status in expectation.expected)
        lines[test] = ExpectationLine(expectation.filename, expectation.line_number, tuple(BUG_RE.findall(text)), expected)
    return lines


def expectation_lines_for(tests, configuration, suite, host=None):
    """Each test's expectation line in the current files for a Results DB configuration; a test no line names is left out."""
    port, device_type = port_for_configuration(configuration, suite, host=host)
    if suite == API_TESTS:
        return _api_test_expectation_lines(port, tests)
    return _layout_test_expectation_lines(port, device_type, tests)


def expectation_line_for(test, configuration, suite, host=None):
    return expectation_lines_for([test], configuration, suite, host=host).get(test)

# Copyright (C) 2025 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import os
from webkitpy.common.host import Host
from webkitpy.port import configuration_options, platform_options, factory
from webkitpy.binary_bundling.ldd import SharedObjectResolver

PLUGIN_NAME = "browser-binary-size"


def generate_json_for_benchmark(basename_sizes):
    benchmark_json = {PLUGIN_NAME: {"metrics": {"Size": ["Total"]}, "tests": {}}}
    test_data_entry = benchmark_json[PLUGIN_NAME]["tests"]
    for browser_object in basename_sizes:
        assert(browser_object not in test_data_entry), f"browser_object {browser_object} should be unique"
        test_data_entry[browser_object] = {"metrics": {"Size": {"current": [basename_sizes[browser_object]]}}}
    return benchmark_json


def get_basenames_and_sizes(path_list):
    basename_sizes = {}
    for object_path in path_list:
        object_base = os.path.basename(object_path)
        if object_base in basename_sizes:
            raise RuntimeError(f'There are repeated objects on the list. Object {object_path} is at least twice')
        basename_sizes[object_base] = os.path.getsize(object_path)
    return basename_sizes


def get_browser_relevant_objects_glib(browser_driver):
    required_binary = 'Tools/Scripts/run-minibrowser'
    if not browser_driver.process_name.endswith(required_binary):
        raise NotImplementedError(f'Getting the browser size data for browser "{browser_name}" is only supported when running the browser via "{required_binary}" and the driver was going to run "{browser_driver.process_name}"')
    browser_relevant_objects = []
    port_name = 'gtk' if browser_driver.browser_name == 'minibrowser-gtk' else 'wpe'
    port_driver = factory.PortFactory(Host()).get(port_name)
    browser_path = port_driver.get_browser_path('cog' if browser_driver.browser_name == 'cog' else 'MiniBrowser')
    browser_relevant_objects.append(browser_path)
    browser_env = port_driver.setup_environ_for_server()
    if not os.path.isfile(browser_path):
        raise ValueError(f"The browser path does not exist: {browser_path}")
    libraries, interpreter = SharedObjectResolver('ldd', browser_env).get_libs_and_interpreter(browser_path)
    browser_build_dir = port_driver._build_path()
    for library in sorted(libraries):
        if library.startswith(browser_build_dir) or os.path.realpath(library).startswith(browser_build_dir):
            browser_relevant_objects.append(library)
    return browser_relevant_objects


def run(browser_driver, results_file):
    if browser_driver.platform == "linux" and (browser_driver.browser_name.startswith('minibrowser-wpe') or browser_driver.browser_name in ['cog', 'minibrowser-gtk']):
        browser_relevant_objects = get_browser_relevant_objects_glib(browser_driver)
    else:
        raise NotImplementedError(f'Getting the browser size data for browser "{browser_driver.browser_name}" and platform "{browser_driver.platform}" is not implemented')

    browser_objects_sizes = get_basenames_and_sizes(browser_relevant_objects)
    benchmark_output = generate_json_for_benchmark(browser_objects_sizes)
    with open(results_file, "w") as f:
        json.dump(benchmark_output, f, indent=2)


# Register the plugin
def register():
    return {
        "name": PLUGIN_NAME,
        "run": run
    }

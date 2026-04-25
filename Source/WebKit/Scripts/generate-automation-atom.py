#!/usr/bin/env python3
#
# Copyright (C) 2022 Igalia S.L.
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

import os
import re
import sys


def collect_utils(data):
    match = re.findall(r'(?P<destination>utils\.[A-Za-z_0-9]+)', data)
    return set(match)


def parse_utils(utils):
    functions = {}
    current_function = None
    with open(utils, "r") as fd:
        for line in fd.readlines():
            if line.startswith("utils."):
                current_function = line.split(" ", 1)[0]
                functions.setdefault(current_function, "")
            elif line.startswith("}"):
                functions[current_function] += line
                current_function = None

            if current_function is not None:
                functions[current_function] += line
    return functions


def append_functions(utils_data, util_functions, util_functions_impl, functions_written):
    util_functions_list = list(util_functions)
    util_functions_list.sort()
    for function in util_functions_list:
        if function in functions_written:
            continue

        function_impl = util_functions_impl[function]
        dependencies = collect_utils(function_impl)
        if function in dependencies:
            dependencies.remove(function)
        append_functions(utils_data, dependencies, util_functions_impl, functions_written)

        for line in function_impl.split("\n"):
            if not line.strip():
                utils_data.append(line)
            else:
                utils_data.append("    " + line)
        functions_written.append(function)


def main(args):
    input = args[0]
    output = args[1]
    utils = os.path.join(os.path.dirname(input), "utils.js")

    with open(input, "r", encoding="utf-8") as fd:
        input_data = fd.read()

    util_functions = collect_utils(input_data)

    if util_functions:
        utils_data = ["    var utils = { };", ""]
        util_functions_impl = parse_utils(utils)
        functions_written = []
        append_functions(utils_data, util_functions, util_functions_impl, functions_written)
        input_data = input_data.replace('"use strict";\n', '"use strict";\n\n' + "\n".join(utils_data))

    with open(output, "w", encoding="utf-8") as fd:
        fd.write(input_data)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

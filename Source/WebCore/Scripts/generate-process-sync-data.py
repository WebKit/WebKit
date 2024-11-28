#!/usr/bin/env python3
#
# Copyright (C) 2024 Apple Inc. All rights reserved.
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

import copy
import os
import re
import sys

_header_license = """/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"""


class SyncedData(object):
    def __init__(self, name, underlying_type_namespace, underlying_type):
        self.name = name
        self.underlying_type_namespace = underlying_type_namespace
        self.underlying_type = underlying_type
        if underlying_type_namespace is None:
            self.fully_qualified_type = underlying_type
        else:
            self.fully_qualified_type = underlying_type_namespace + '::' + underlying_type


def parse_process_sync_data(file):
    synched_datas = []
    headers = []
    for line in file:
        line = line.strip()

        # Skip comments
        if line.startswith('#'):
            continue

        match = re.search(r'^headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(header)
            continue

        match = re.search(r'(.*) : (.*)::(.*)', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], match.groups()[1], match.groups()[2]))
            continue

        match = re.search(r'(.*) : (.*)', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], None, match.groups()[1]))
            continue

    return [synched_datas, headers]


_process_sync_client_header_prefix = """
namespace WebCore {

struct ProcessSyncData;

class ProcessSyncClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ProcessSyncClient);

public:
    ProcessSyncClient() = default;
    virtual ~ProcessSyncClient() = default;
"""

_process_sync_client_header_suffix = """
protected:
    virtual void broadcastProcessSyncDataToOtherProcesses(const ProcessSyncData&) { }
};

} // namespace WebCore
"""


def generate_process_sync_client_header(synched_datas, headers):
    result = []
    result.append(_header_license)
    result.append('#pragma once\n')

    headers_set = set(headers)
    headers_set.add('<wtf/TZoneMallocInlines.h>')
    headers = sorted(list(headers_set))
    for header in headers:
        result.append('#include %s' % header)

    result.append(_process_sync_client_header_prefix)

    for data in sorted(synched_datas, key=lambda data: data.name):
        result.append('    void broadcast%sToOtherProcesses(const %s&);' % (data.name, data.fully_qualified_type))

    result.append(_process_sync_client_header_suffix)
    return '\n'.join(result)


_process_sync_client_impl_prefix = """
#include "config.h"
#include "ProcessSyncClient.h"

#include "ProcessSyncData.h"

namespace WebCore {
"""


def generate_process_sync_client_impl(synched_datas):
    result = []
    result.append(_header_license)
    result.append(_process_sync_client_impl_prefix)

    for data in sorted(synched_datas, key=lambda data: data.name):
        result.append('void ProcessSyncClient::broadcast%sToOtherProcesses(const %s& data)' % (data.name, data.fully_qualified_type))
        result.append('{')
        result.append('    broadcastProcessSyncDataToOtherProcesses({ ProcessSyncDataType::%s, { data }});' % data.name)
        result.append('}')

    result.append('\n} // namespace WebCore\n')
    return '\n'.join(result)


_process_sync_data_header_prefix = """
#pragma once

namespace WebCore {
"""

_process_sync_data_header_suffix = """
struct ProcessSyncData {
    ProcessSyncDataType type;
    ProcessSyncDataVariant value;
};

}; // namespace WebCore
"""


def sorted_qualified_types(synched_datas):
    variant_type_set = set()
    for data in synched_datas:
        variant_type_set.add(data.fully_qualified_type)
    return sorted(list(variant_type_set))


def generate_process_sync_data_header(synched_datas):
    result = []
    result.append(_header_license)
    result.append(_process_sync_data_header_prefix)

    result.append("enum class ProcessSyncDataType : uint8_t {")
    for data in sorted(synched_datas, key=lambda data: data.name):
        result.append('    %s,' % data.name)
    result.append("};")
    result.append(" ")

    result.append("using ProcessSyncDataVariant = std::variant<")

    sorted_variant_types = sorted_qualified_types(synched_datas)
    for variant_type in sorted_variant_types[:-1]:
        result.append('    %s,' % variant_type)
    result.append('    %s' % sorted_variant_types[-1])
    result.append(">;")
    result.append(_process_sync_data_header_suffix)
    return '\n'.join(result)


_serialization_in_license = """#
# Copyright (C) 2024 Apple Inc. All rights reserved.
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
#
"""
_process_sync_data_serialiation_in_prefix = """
header: <WebCore/ProcessSyncData.h>
"""
_process_sync_data_serialiation_in_suffix = """
struct WebCore::ProcessSyncData {
    WebCore::ProcessSyncDataType type;
    WebCore::ProcessSyncDataVariant value;
};
"""


def generate_process_sync_data_serialiation_in(synched_datas):
    result = []
    result.append(_serialization_in_license)
    result.append(_process_sync_data_serialiation_in_prefix)

    result.append("enum class WebCore::ProcessSyncDataType : uint8_t {")
    for data in sorted(synched_datas, key=lambda data: data.name):
        result.append('    %s,' % data.name)
    result.append("};")
    result.append(" ")

    sorted_variant_types = sorted_qualified_types(synched_datas)

    variant_string = "using WebCore::ProcessSyncDataVariant = std::variant<"
    for variant_type in sorted_variant_types[:-1]:
        variant_string += variant_type + ', '
    variant_string += sorted_variant_types[-1] + '>;'

    result.append(variant_string)
    result.append(_process_sync_data_serialiation_in_suffix)
    return '\n'.join(result)


def main(argv):
    synched_datas = []
    headers = []

    if len(argv) < 2:
        return -1

    output_directory = ''
    if len(argv) > 2:
        output_directory = argv[2] + '/'

    with open(argv[1]) as file:
        new_synched_datas, new_headers = parse_process_sync_data(file)
        for synched_data in new_synched_datas:
            synched_datas.append(synched_data)
        header_set = set()
        for header in new_headers:
            header_set.add(header)
        headers = sorted(list(header_set))

    with open(output_directory + 'ProcessSyncClient.h', "w+") as output:
        output.write(generate_process_sync_client_header(synched_datas, headers))

    with open(output_directory + 'ProcessSyncClient.cpp', "w+") as output:
        output.write(generate_process_sync_client_impl(synched_datas))

    with open(output_directory + 'ProcessSyncData.h', "w+") as output:
        output.write(generate_process_sync_data_header(synched_datas))

    with open(output_directory + 'ProcessSyncData.serialization.in', "w+") as output:
        output.write(generate_process_sync_data_serialiation_in(synched_datas))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

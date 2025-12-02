#!/usr/bin/env python3
#
# Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
    def __init__(self, name, underlying_type_namespace, underlying_type, options):
        self.conditional = None
        self.header = None
        self.variant_index = None

        if options is not None:
            option_list = options.split()
            for option in option_list:
                if option.startswith('Conditional='):
                    self.conditional = option[12:]
                elif option.startswith('Header='):
                    self.header = option[7:]
                else:
                    raise Exception("Invalid option argument '%s' found" % option)

        self.name = name
        self.underlying_type_namespace = underlying_type_namespace
        self.underlying_type = underlying_type
        if underlying_type_namespace is None:
            self.fully_qualified_type = underlying_type
        else:
            self.fully_qualified_type = underlying_type_namespace + '::' + underlying_type


def headers_from_datas(datas):
    header_list = []
    for data in datas:
        if data.header is None:
            continue
        header_list.append(data.header)
    return header_list


def parse_process_sync_data(file):
    synched_datas = []
    headers = []
    for line in file:
        line = line.strip()

        # Skip comments
        if line.startswith('#'):
            continue

        match = re.search(r'(.*) : (.*)::(.*) \[(.*)\]', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], match.groups()[1], match.groups()[2], match.groups()[3]))
            continue

        match = re.search(r'(.*) : (.*)::(.*)', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], match.groups()[1], match.groups()[2], None))
            continue

        match = re.search(r'(.*) : (.*) \[(.*)\]', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], None, match.groups()[1], match.groups()[2]))
            continue

        match = re.search(r'(.*) : (.*)', line)
        if match:
            synched_datas.append(SyncedData(match.groups()[0], None, match.groups()[1], None))
            continue

    return synched_datas


_process_sync_client_header_prefix = """
namespace WebCore {{

class {prefix}SyncData;
struct {prefix}SyncSerializationData;

class {prefix}SyncClient {{
    WTF_MAKE_TZONE_ALLOCATED_INLINE({prefix}SyncClient);

public:
    {prefix}SyncClient() = default;
    virtual ~{prefix}SyncClient() = default;

    virtual void broadcastAll{prefix}SyncDataToOtherProcesses({prefix}SyncData&) {{ }}
"""

_process_sync_client_header_suffix = """
protected:
    virtual void broadcast{prefix}SyncDataToOtherProcesses(const {prefix}SyncSerializationData&) {{ }}
}};

}} // namespace WebCore
"""


def generate_process_sync_client_header(prefix, synched_datas):
    result = []
    result.append(_header_license)
    result.append('#pragma once\n')

    headers = headers_from_datas(synched_datas)
    headers.append('<wtf/TZoneMallocInlines.h>')
    for header in headers:
        result.append('#include %s' % header)

    result.append(_process_sync_client_header_prefix.format(prefix=prefix))

    for data in synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('    void broadcast%sToOtherProcesses(const %s&);' % (data.name, data.fully_qualified_type))
        if data.conditional is not None:
            result.append('#endif')

    result.append(_process_sync_client_header_suffix.format(prefix=prefix))
    return '\n'.join(result)


_process_sync_client_impl_prefix = """
#include "config.h"
#include "{prefix}SyncClient.h"

#include "{prefix}SyncData.h"
#include <wtf/EnumTraits.h>

namespace WebCore {{
"""


def generate_process_sync_client_impl(prefix, synched_datas):
    result = []
    result.append(_header_license)
    result.append(_process_sync_client_impl_prefix.format(prefix=prefix))

    for data in synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('void %sSyncClient::broadcast%sToOtherProcesses(const %s& data)' % (prefix, data.name, data.fully_qualified_type))
        result.append('{')
        result.append('    %sSyncDataVariant dataVariant;' % (prefix))
        result.append('    dataVariant.emplace<enumToUnderlyingType(%sSyncDataType::%s)>(data);' % (prefix, data.name))
        result.append('    broadcast%sSyncDataToOtherProcesses({ %sSyncDataType::%s, WTFMove(dataVariant) });' % (prefix, prefix, data.name))
        result.append('}')
        if data.conditional is not None:
            result.append('#endif')

    result.append('\n} // namespace WebCore\n')
    return '\n'.join(result)


_process_sync_data_header_suffix = """
struct {prefix}SyncSerializationData {{
    {prefix}SyncDataType type;
    {prefix}SyncDataVariant value;
}};

"""


def generate_process_sync_data_header(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas):
    result = []

    result.append("enum class %sSyncDataType : uint8_t {" % prefix)
    for data in variant_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('    %s = %s,' % (data.name, data.variant_index))
        if data.conditional is not None:
            result.append('#endif')

    result.append("};")
    result.append("")

    result.append("static const %sSyncDataType all%sSyncDataTypes[] = {" % (prefix, prefix))
    data = sync_data_sorted_synched_datas[0]
    if data.conditional is not None:
        result.append('#if %s' % data.conditional)
    result.append('    %sSyncDataType::%s' % (prefix, data.name))
    if data.conditional is not None:
        result.append('#endif')

    for data in sync_data_sorted_synched_datas[1:]:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('    , %sSyncDataType::%s' % (prefix, data.name))
        if data.conditional is not None:
            result.append('#endif')
    result.append("};")
    result.append("")

    for data in variant_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if !%s' % data.conditional)
            result.append('using %s = bool;' % data.underlying_type)
            result.append('#endif')

    result.append("")
    result.append("using %sSyncDataVariant = Variant<" % prefix)
    for data in variant_sorted_synched_datas[:-1]:
        result.append('    %s,' % data.fully_qualified_type)

    data = variant_sorted_synched_datas[-1]
    result.append('    %s' % data.fully_qualified_type)

    result.append(">;")
    result.append(_process_sync_data_header_suffix.format(prefix=prefix))
    return '\n'.join(result)


_synced_data_header_midfix = """
namespace WebCore {{

struct {prefix}SyncSerializationData;

class data_type_name : public RefCounted<data_type_name> {{
WTF_MAKE_TZONE_ALLOCATED_INLINE(data_type_name);
public:
    template<typename... Args>
    static Ref<data_type_name> create(Args&&... args)
    {{
        return adoptRef(*new data_type_name(std::forward<Args>(args)...));
    }}
    static Ref<data_type_name> create() {{ return adoptRef(*new data_type_name); }}
    void update(const {prefix}SyncSerializationData&);
"""


def generate_synched_data_header(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas):
    result = []
    result.append(_header_license)
    result.append('#pragma once\n')

    data_type_name = prefix + 'SyncData'

    headers = []
    headers.append('<wtf/TZoneMallocInlines.h>')
    headers.append('<wtf/Ref.h>')
    headers.append('<wtf/RefCounted.h>')
    for data in sync_data_sorted_synched_datas:
        if data.header is None:
            continue
        headers.append(data.header)

    for header in headers:
        result.append('#include %s' % header)

    result.append(_synced_data_header_midfix.format(prefix=prefix))

    for data in sync_data_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        name = data.name[0].lower() + data.name[1:]
        result.append('    %s %s = { };' % (data.fully_qualified_type, name))
        if data.conditional is not None:
            result.append('#endif')

    result.append('')
    result.append('private:')
    result.append('    data_type_name() = default;')
    result.append('    WEBCORE_EXPORT data_type_name(')

    data = sync_data_sorted_synched_datas[0]
    if data.conditional is not None:
        result.append('#if %s' % data.conditional)
    result.append('        %s' % data.fully_qualified_type)
    if data.conditional is not None:
        result.append('#endif')

    for data in sync_data_sorted_synched_datas[1:]:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('      , %s' % data.fully_qualified_type)
        if data.conditional is not None:
            result.append('#endif')

    result.append('    );')
    result.append('};')
    result.append('')

    result.append(generate_process_sync_data_header(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas))

    result.append('} // namespace WebCore')
    result.append('')

    return '\n'.join(result).replace('data_type_name', data_type_name)


_synched_data_impl_prefix = """
#include "config.h"
#include "data_type_name.h"

#include <wtf/EnumTraits.h>

namespace WebCore {{

void data_type_name::update(const {prefix}SyncSerializationData& data)
{{
    switch (data.type) {{"""

_synched_data_impl_midfix = """    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
"""


def generate_synched_data_impl(prefix, synched_datas):
    result = []
    result.append(_header_license)
    result.append(_synched_data_impl_prefix.format(prefix=prefix))

    data_type_name = prefix + 'SyncData'

    for data in synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)

        lowercase_name = data.name[0].lower() + data.name[1:]
        result.append('    case %sSyncDataType::%s:' % (prefix, data.name))
        result.append('        %s = std::get<enumToUnderlyingType(%sSyncDataType::%s)>(data.value);' % (lowercase_name, prefix, data.name))
        result.append('        break;')

        if data.conditional is not None:
            result.append('#endif')

    result.append(_synched_data_impl_midfix)

    result.append('data_type_name::data_type_name(')

    data = synched_datas[0]
    if data.conditional is not None:
        raise Exception("First argument to constructor cannot be conditional")

    lowercase_name = data.name[0].lower() + data.name[1:]
    result.append('      %s %s' % (data.fully_qualified_type, lowercase_name))

    for data in synched_datas[1:]:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)

        lowercase_name = data.name[0].lower() + data.name[1:]
        result.append('    , %s %s' % (data.fully_qualified_type, lowercase_name))

        if data.conditional is not None:
            result.append('#endif')

    result.append(')')

    data = synched_datas[0]
    if data.conditional is not None:
        raise Exception("First argument to constructor cannot be conditional")

    lowercase_name = data.name[0].lower() + data.name[1:]
    result.append('    : %s(%s)' % (lowercase_name, lowercase_name))

    for data in synched_datas[1:]:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)

        lowercase_name = data.name[0].lower() + data.name[1:]
        result.append('    , %s(%s)' % (lowercase_name, lowercase_name))

        if data.conditional is not None:
            result.append('#endif')

    result.append('{')
    result.append('}')
    result.append('')
    result.append('} // namespace WebCore')
    result.append('')

    return '\n'.join(result).replace('data_type_name', data_type_name)


_serialization_in_license = """#
# Copyright (C) 2025 Apple Inc. All rights reserved.
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
_process_sync_data_serialization_in_prefix = """
header: <WebCore/{prefix}SyncData.h>
"""

_process_sync_data_serialization_in_suffix = """
[CustomHeader] struct WebCore::{prefix}SyncSerializationData {{
    WebCore::{prefix}SyncDataType type;
    WebCore::{prefix}SyncDataVariant value;
}};
"""


def generate_process_sync_data_serialiation_in(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas):
    result = []
    result.append(_serialization_in_license)
    result.append(_process_sync_data_serialization_in_prefix.format(prefix=prefix))

    result.append('[RefCounted] class WebCore::%sSyncData {' % (prefix))
    for data in sync_data_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        name = data.name[0].lower() + data.name[1:]
        result.append('    %s %s;' % (data.fully_qualified_type, name))
        if data.conditional is not None:
            result.append('#endif')

    result.append('};')
    result.append('')

    result.append("enum class WebCore::%sSyncDataType : uint8_t {" % prefix)
    for data in variant_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if %s' % data.conditional)
        result.append('    %s,' % data.name)
        if data.conditional is not None:
            result.append('#endif')

    result.append("};")
    result.append(" ")

    for data in variant_sorted_synched_datas:
        if data.conditional is not None:
            result.append('#if !%s' % data.conditional)
            result.append('using %s = bool;' % data.fully_qualified_type)
            result.append('#endif')

    result.append("")
    variant_string = "using WebCore::%sSyncDataVariant = Variant<" % prefix
    for data in variant_sorted_synched_datas[:-1]:
        variant_string += data.fully_qualified_type + ', '
    variant_string += variant_sorted_synched_datas[-1].fully_qualified_type + '>;'
    result.append(variant_string)

    result.append(_process_sync_data_serialization_in_suffix.format(prefix=prefix))
    return '\n'.join(result)


def sort_data_lists(synched_datas):
    type_list = []
    conditional_type_list = []

    for data in synched_datas:
        if data.conditional is None:
            type_list.append(data)
        else:
            conditional_type_list.append(data)

    return type_list, conditional_type_list


def sort_datas_for_variant_order(synched_datas):
    type_list, conditional_type_list = sort_data_lists(synched_datas)

    if not type_list:
        raise Exception("Surprisingly, no unconditional types found (this will make it hard to construct the variant in a way that will compile)")

    full_list = conditional_type_list + type_list
    current_variant_index = 0
    for data in full_list:
        data.variant_index = current_variant_index
        current_variant_index += 1

    return full_list


def sort_datas_for_sync_data_order(synched_datas):
    type_list, conditional_type_list = sort_data_lists(synched_datas)

    # FIXME: We play tricks with our c++ native interface and implementation of DocumentSyncData to put commas at the start of the line and not the end,
    # allowing us to gracefully handle the case where the final member is conditional with proper syntax.
    # However, the serializer generator up at the WebKit layer does *not* play these tricks, and therefore if the final member is conditional and not defined for
    # the platform, the build fails.
    # We should fix the serializer generator to use leading commas instead of trailing commas then remove this weird construct of making sure the conditionals
    # are sandwiched between unconditionals.
    if len(type_list) > 1:
        full_list = type_list[:-1] + conditional_type_list + [type_list[-1]]
    else:
        full_list = type_list
    return full_list


def main(argv):
    synched_datas = []
    headers = []

    if len(argv) < 3:
        return -1

    output_directory = ''
    if len(argv) > 3:
        output_directory = argv[3] + '/'

    prefix = argv[1]

    with open(argv[2]) as file:
        new_synched_datas = parse_process_sync_data(file)
        for synched_data in new_synched_datas:
            synched_datas.append(synched_data)

    variant_sorted_synched_datas = sort_datas_for_variant_order(synched_datas)
    sync_data_sorted_synched_datas = sort_datas_for_sync_data_order(synched_datas)

    with open(output_directory + prefix + 'SyncClient.h', "w+") as output:
        output.write(generate_process_sync_client_header(prefix, variant_sorted_synched_datas))

    with open(output_directory + prefix + 'SyncClient.cpp', "w+") as output:
        output.write(generate_process_sync_client_impl(prefix, variant_sorted_synched_datas))

    sorted_synched_datas = sort_datas_for_sync_data_order(synched_datas)

    with open(output_directory + prefix + 'SyncData.serialization.in', "w+") as output:
        output.write(generate_process_sync_data_serialiation_in(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas))

    with open(output_directory + prefix + 'SyncData.h', "w+") as output:
        output.write(generate_synched_data_header(prefix, variant_sorted_synched_datas, sync_data_sorted_synched_datas))

    with open(output_directory + prefix + 'SyncData.cpp', "w+") as output:
        output.write(generate_synched_data_impl(prefix, sync_data_sorted_synched_datas))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

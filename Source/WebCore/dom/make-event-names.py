#!/usr/bin/env python3
#
# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json


def lowercase_first_letter(string):
    return string[0].lower() + string[1:] if len(string) and string[1].islower() else string


def main():
    parser = argparse.ArgumentParser(description='Generate EventNames.h')
    parser.add_argument('--event-names')
    args = parser.parse_args()

    with open(args.event_names, 'r', encoding='utf-8') as event_names_file:
        event_names_input = json.load(event_names_file)

    with open('EventNames.h', 'w') as output_file:
        def write(text):
            output_file.write(text)

        def writeln(text):
            write(text + '\n')

        writeln('''/*
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "ThreadGlobalData.h"
#include <array>
#include <functional>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct EventNames {
    WTF_MAKE_NONCOPYABLE(EventNames); WTF_MAKE_FAST_ALLOCATED;

public:''')
        type_map = {}
        for name in sorted(event_names_input.keys()):
            entry = event_names_input[name]
            conditional = entry.get('conditional', None)
            if conditional:
                writeln(f'#if {conditional}')
            writeln(f'    const AtomString {name}Event;')
            for event_type in entry.get('types', []):
                type_map.setdefault(event_type, [])
                type_map[event_type].append(name)
            if conditional:
                writeln('#endif')

        writeln('''
    template<class... Args>
    static std::unique_ptr<EventNames> create(Args&&... args)
    {
        return std::unique_ptr<EventNames>(new EventNames(std::forward<Args>(args)...));
    }''')

        for event_type in sorted(type_map.keys()):
            writeln('')
            writeln(f'    inline bool is{event_type}EventType(const AtomString&) const;')
            name_count = len(type_map[event_type])
            writeln(f'    inline std::array<const AtomString, {name_count}> {lowercase_first_letter(event_type)}EventNames() const;')

        writeln('')
        writeln(f'    inline std::array<const AtomString, {len(event_names_input)}> allEventNames() const;')

        writeln('''
private:
    EventNames();
};

const EventNames& eventNames();

inline const EventNames& eventNames()
{
    return threadGlobalData().eventNames();
}''')

        for event_type in sorted(type_map.keys()):
            names = type_map[event_type]
            name_count = len(type_map[event_type])
            writeln('')
            writeln(f'inline bool EventNames::is{event_type}EventType(const AtomString& type) const')
            writeln('{')
            # FIXME: Support conditionals in the first or the last item.
            writeln(f'    return type == {names[0]}Event')
            for name in names[1:-1]:
                conditional = event_names_input[name].get('conditional', None)
                if conditional:
                    writeln(f'#if {conditional}')
                writeln(f'        || type == {name}Event')
                if conditional:
                    writeln(f'#endif')
            if len(names) >= 2:
                writeln(f'        || type == {names[-1]}Event;')
            writeln('}')
            writeln('')
            writeln(f'inline std::array<const AtomString, {name_count}> EventNames::{lowercase_first_letter(event_type)}EventNames() const')
            writeln('{')
            writeln('    return { {')
            for name in names:
                conditional = event_names_input[name].get('conditional', None)
                if conditional:
                    writeln(f'#if {conditional}')
                writeln(f'        {name}Event,')
                if conditional:
                    writeln(f'#endif')
            writeln('    } };')
            writeln('}')

        writeln('')

        writeln(f'inline std::array<const AtomString, {len(event_names_input)}> EventNames::allEventNames() const')
        writeln('{')
        writeln('    return { {')
        for name in sorted(event_names_input.keys()):
            conditional = event_names_input[name].get('conditional', None)
            if conditional:
                writeln(f'#if {conditional}')
            writeln(f'        {name}Event,')
            if conditional:
                writeln(f'#endif')
        writeln('    } };')
        writeln('}')
        writeln('')
        writeln('} // namespace WebCore')

    with open('EventNames.cpp', 'w') as output_file:
        def writeln(text):
            output_file.write(text + '\n')
        writeln('''/*
 * Copyright (C) 2005-2024 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "EventNames.h"

namespace WebCore {

EventNames::EventNames()''')

        delimiter = ':'
        for name in sorted(event_names_input.keys()):
            conditional = event_names_input[name].get('conditional', None)
            if conditional:
                writeln(f'#if {conditional}')
            writeln(f'    {delimiter} {name}Event(\"{name}\"_s)')
            delimiter = ','
            if conditional:
                writeln('#endif')

        writeln('''{ }

} // namespace WebCore''')


if __name__ == '__main__':
    main()

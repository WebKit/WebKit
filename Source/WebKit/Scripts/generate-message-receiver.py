#!/usr/bin/env python3
#
# Copyright (C) 2010 Apple Inc. All rights reserved.
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

from __future__ import with_statement
import argparse
import os
import sys

import webkit.messages
import webkit.parser
import webkit.model

def main(argv):
    parser = argparse.ArgumentParser(description='Generate message receivers from input files')
    parser.add_argument('base_dir', help='Base directory for message receiver files')
    parser.add_argument('message_receivers', nargs='+', help='Message receiver files to process')
    parser.add_argument('--output-dir', help='Directory for output files')

    args = parser.parse_args(argv[1:])

    receivers = []
    output_dir = args.output_dir
    base_dir = args.base_dir
    receiver_header_files = []

    for message_receiver in args.message_receivers:
        receiver_name = message_receiver.rsplit('/', 1).pop()

        if os.path.exists('%s/%s.messages.in' % (os.getcwd(), message_receiver)):
            with open('%s/%s.messages.in' % (os.getcwd(), message_receiver)) as source_file:
                receiver = webkit.parser.parse(source_file)
        else:
            with open('%s/%s.messages.in' % (base_dir, message_receiver)) as source_file:
                receiver = webkit.parser.parse(source_file)

        receiver.enforce_attribute_constraints()
        receiver.enforce_opaque_ipc_types_usage()

        receivers.append(receiver)
        if receiver_name != receiver.name:
            sys.stderr.write("Error: %s defined in file %s/%s.messages.in instead of %s.messages.in\n" % (receiver.name, base_dir, message_receiver, receiver.name))
            sys.exit(1)

    errors = webkit.model.check_global_model_inputs(receivers)
    if errors:
        for e in errors:
            sys.stderr.write("Error: %s\n" % e)
        sys.exit(1)

    receivers = webkit.model.generate_global_model(receivers)

    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    def output_path(filename):
        if output_dir:
            return os.path.join(output_dir, filename)
        return filename

    for receiver in receivers:
        if receiver.has_attribute(webkit.model.BUILTIN_ATTRIBUTE):
            continue
        with open(output_path('%sMessageReceiver.cpp' % receiver.name), "w+") as implementation_output:
            implementation_output.write(webkit.messages.generate_message_handler(receiver))
        if receiver.swift_receiver or receiver.swift_receiver_build_enabled_by:
            with open(output_path('%sMessageReceiver.swift' % receiver.name), "w+") as swift_implementation_output:
                swift_implementation_output.write(webkit.messages.generate_swift_message_handler(receiver))

        receiver_message_header = '%sMessages.h' % receiver.name
        receiver_header_files.append(receiver_message_header)
        with open(output_path(receiver_message_header), "w+") as header_output:
            header_output.write(webkit.messages.generate_messages_header(receiver))

    with open(output_path('MessageNames.h'), "w+") as message_names_header_output:
        message_names_header_output.write(webkit.messages.generate_message_names_header(receivers))

    with open(output_path('MessageNames.cpp'), "w+") as message_names_implementation_output:
        message_names_implementation_output.write(webkit.messages.generate_message_names_implementation(receivers))

    with open(output_path('MessageArgumentDescriptions.cpp'), "w+") as message_descriptions_implementation_output:
        message_descriptions_implementation_output.write(webkit.messages.generate_message_argument_description_implementation(receivers, receiver_header_files))

    with open(output_path('module.private.modulemap'), "w+") as modulemap_output:
        modulemap_output.write(webkit.messages.generate_modulemap(receiver_header_files))

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))

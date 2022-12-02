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
import sys

import webkit.messages
import webkit.parser
import webkit.model

def main(argv):
    receivers = []

    first_arg = True
    second_arg = False
    receiver_header_files = []
    for parameter in argv:
        if first_arg:
            first_arg = False
            second_arg = True
            continue
        if second_arg:
            base_dir = parameter
            second_arg = False
            continue

        receiver_name = parameter.rsplit('/', 1).pop()

        with open('%s/%s.messages.in' % (base_dir, parameter)) as source_file:
            receiver = webkit.parser.parse(source_file)
        receivers.append(receiver)
        if receiver_name != receiver.name:
            sys.stderr.write("Error: %s defined in file %s/%s.messages.in instead of %s.messages.in\n" % (receiver.name, base_dir, parameter, receiver.name))
            sys.exit(1)

    errors = webkit.model.check_global_model_inputs(receivers)
    if errors:
        for e in errors:
            sys.stderr.write("Error: %s\n" % e)
        sys.exit(1)

    receivers = webkit.model.generate_global_model(receivers)

    for receiver in receivers:
        if receiver.has_attribute(webkit.model.BUILTIN_ATTRIBUTE):
            continue
        with open('%sMessageReceiver.cpp' % receiver.name, "w+") as implementation_output:
            implementation_output.write(webkit.messages.generate_message_handler(receiver))

        receiver_message_header = '%sMessages.h' % receiver.name
        receiver_header_files.append(receiver_message_header)
        with open(receiver_message_header, "w+") as header_output:
            header_output.write(webkit.messages.generate_messages_header(receiver))

    with open('MessageNames.h', "w+") as message_names_header_output:
        message_names_header_output.write(webkit.messages.generate_message_names_header(receivers))

    with open('MessageNames.cpp', "w+") as message_names_implementation_output:
        message_names_implementation_output.write(webkit.messages.generate_message_names_implementation(receivers))

    with open('MessageArgumentDescriptions.cpp', "w+") as message_descriptions_implementation_output:
        message_descriptions_implementation_output.write(webkit.messages.generate_message_argument_description_implementation(receivers, receiver_header_files))

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))

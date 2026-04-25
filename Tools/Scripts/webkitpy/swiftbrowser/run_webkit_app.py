#!/usr/bin/env python3

# Copyright (C) 2025 Apple Inc. All rights reserved.
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
import sys
import traceback

from webkitpy.common.host import Host
from webkitpy.port import configuration_options, platform_options, factory
from webkitcorepy.string_utils import decode


def main(argv):
    option_parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter, usage="%(prog)s [options] [url]")
    groups = [("Platform options", platform_options()), ("Configuration options", configuration_options())]

    # Convert options to argparse, so that we can use parse_known_args() which is not supported in optparse.
    # FIXME: Globally migrate to argparse. https://bugs.webkit.org/show_bug.cgi?id=213463
    for group_name, group_options in groups:
        option_group = option_parser.add_argument_group(group_name)

        for option in group_options:
            # Skip deprecated option
            if option.get_opt_string() != "--target":
                default = None
                if option.default != ("NO", "DEFAULT"):
                    default = option.default
                option_group.add_argument(option.get_opt_string(), action=option.action, dest=option.dest,
                                          help=option.help, const=option.const, default=default)

    option_parser.add_argument('url', metavar='url', type=lambda s: decode(s, 'utf8'), nargs='?', help='Website URL to load')
    options, _ = option_parser.parse_known_args(argv)

    if not options.platform:
        options.platform = "mac"

    try:
        port = factory.PortFactory(Host()).get(options.platform, options=options)
        return port.run_swiftbrowser(["--url", options.url] if options.url else [])
    except BaseException as e:
        if isinstance(e, Exception):
            print('\n%s raised: %s' % (e.__class__.__name__, str(e)), file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

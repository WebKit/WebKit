# pylint: disable=E1103
# Copyright (C) 2020 Igalia S.L.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.

from __future__ import print_function
import argparse
import os
import sys
import traceback

from webkitpy.common.host import Host
from webkitpy.port import configuration_options, platform_options, factory


def main(argv):
    option_parser = argparse.ArgumentParser(usage="%(prog)s [options] [url]", add_help=False)
    groups = [("Platform options", platform_options()), ("Configuration options", configuration_options())]

    # Convert options to argparse, so that we can use parse_known_args() which is not supported in optparse.
    # FIXME: Globally migrate to argparse. https://bugs.webkit.org/show_bug.cgi?id=213463
    for group_name, group_options in groups:
        option_group = option_parser.add_argument_group(group_name)

        for option in group_options:
            # Skip deprecated option
            if option.dest == "configuration":
                continue
            default = None
            if option.default != ("NO", "DEFAULT"):
                default = option.default
            option_group.add_argument(option.get_opt_string(), action=option.action, dest=option.dest,
                                      help=option.help, const=option.const, default=default)

    options, args = option_parser.parse_known_args(argv)

    if set(args).issubset(["-h", "--help"]) and not options.platform:
        option_parser.print_help()
        print("\nTo see the available options on a specific platform, supply it on the command-line, for example --gtk --help")
        return 0

    try:
        port = factory.PortFactory(Host()).get(options.platform, options=options)
        return port.run_minibrowser(args)
    except BaseException as e:
        if isinstance(e, Exception):
            print('\n%s raised: %s' % (e.__class__.__name__, str(e)), file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

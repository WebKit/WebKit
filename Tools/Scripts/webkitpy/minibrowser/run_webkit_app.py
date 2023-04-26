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
import shlex
import sys
import traceback

from webkitpy.common.host import Host
from webkitpy.port import configuration_options, platform_options, minibrowser_options, factory
from webkitcorepy.string_utils import decode

def main(argv):
    option_parser = argparse.ArgumentParser(usage="%(prog)s [options] [url]")
    groups = [("Platform options", platform_options()),
              ("Configuration options", configuration_options()),
              ("Minibrowser options", minibrowser_options())]

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

    option_parser.add_argument('url', metavar='url', type=lambda s: decode(s, 'utf8'), nargs='?',
                               help='Website URL to load')
    options, args = option_parser.parse_known_args(argv)

    if not options.platform:
        options.platform = "mac"

    browser_args = []
    extra_minibrowser_args = os.environ.get('WEBKIT_MINI_BROWSER_ARGS', None) if options.minibrowser_args is None else options.minibrowser_args
    if extra_minibrowser_args is not None:
        browser_args.extend(shlex.split(extra_minibrowser_args))
    # Convert unregistered command-line arguments to utf-8 and append parsed
    # URL. convert_arg_line_to_args() returns a list containing a single
    # string, so it needs to be split again.
    browser_args.extend([decode(s, "utf-8") for s in option_parser.convert_arg_line_to_args(' '.join(args))[0].split()])

    if options.url:
        browser_args.append(options.url)

    try:
        port = factory.PortFactory(Host()).get(options.platform, options=options)
        return port.run_minibrowser(browser_args, options.minibrowser_name)
    except BaseException as e:
        if isinstance(e, Exception):
            print('\n%s raised: %s' % (e.__class__.__name__, str(e)), file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/python3

import filecmp
import os
import re


def generate_umbrella_header(input_filename, output_filename, tempoutput_filename, header_dirname, framework_name):
    include_pattern = re.compile(r"\#(include|import)\s+\<" + framework_name + r"\/(.*)\>")

    existing_framework_headers = set()
    output_headers = []
    blocklist = {
        "%sPrivate.h" % framework_name,
        "%s.h" % framework_name,
    }

    with open(tempoutput_filename or output_filename, "w+") as output_file:
        with open(input_filename, "r") as input_file:
            for line in input_file:
                match = include_pattern.search(line)
                if match:
                    existing_framework_headers.add(match.group(2))

                output_file.write(line)

        for (dirpath, dirnames, filenames) in os.walk(header_dirname):
            for file in filenames:
                path = os.path.relpath(os.path.join(dirpath, file), start=header_dirname)

                if path in blocklist or path in existing_framework_headers or not path.endswith(".h"):
                    continue

                output_headers.append(path)

        output_headers.sort()
        output_file.write("\n\n")

        for output_header in output_headers:
            output_file.write("#import <%s/%s>\n" % (framework_name, output_header))

    if tempoutput_filename and (not os.path.exists(output_filename) or not filecmp.cmp(output_filename, tempoutput_filename)):
        os.rename(tempoutput_filename, output_filename)


if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=str, default=None, help="Input header file (ie. ${SRCROOT}/WebKitPrivate.h)")
    parser.add_argument("--output", type=str, default=None, help="Output header file (ie. ${DSTROOT}/WebKit.framework/PrivateHeaders/WebKitPrivate.h)")
    parser.add_argument("--tempoutput", type=str, default=None, help="Temporary output file (ie. ${TARGET_TEMP_DIR}/WebKitPrivate.h\nIf set, the header will be generated to this path, and the output path will only be touched if the file contents have changed.")
    parser.add_argument("--headers", type=str, default=None, help="Header directory (ie. ${DSTROOT}/WebKit.framework/PrivateHeaders)")
    parser.add_argument("--framework", type=str, default=None, help="Framework name (ie. WebKit)")
    args = parser.parse_args()

    if args.input is None or args.output is None or args.headers is None or args.framework is None:
        parser.print_help()
        sys.exit(1)

    generate_umbrella_header(args.input, args.output, args.tempoutput, args.headers, args.framework)

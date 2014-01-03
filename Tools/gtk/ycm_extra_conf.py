#!/usr/bin/env python
# Copyright (C) 2013 Danilo Cesar Lemes de Paula <danilo.eu@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import os
import re
import subprocess
import sys

# It's very likely that this script is a symlink somewhere in the WebKit directory,
# so we try to find the actual script location so that we can locate the tools
# directory.
original_file = __file__[:-1] if __file__.endswith(".pyc") else __file__
if os.path.islink(original_file):
    parent_folder = os.path.abspath(os.path.dirname(original_file))
    link_file = os.path.join(parent_folder, os.readlink(original_file))
    __tools_directory = os.path.dirname(link_file)
else:
    __tools_directory = os.path.dirname(original_file)

sys.path.insert(0, os.path.abspath(__tools_directory))
import common

MAKE_TRACE_FILE_NAME = "ycm-make-trace"
FLAGS_PRECEDING_PATHS = ['-isystem', '-I', '-iquote', '--sysroot=']


def transform_relative_paths_to_absolute_paths(arguments, build_path):
    result = []

    make_next_absolute = False
    for argument in arguments:
        if make_next_absolute:
            make_next_absolute = False
            if not argument.startswith('/'):
                argument = os.path.join(build_path, argument)
        elif argument in FLAGS_PRECEDING_PATHS:
            # Some flags precede the path in the list. For those we make the
            # next argument absolute.
            if argument == flag:
                make_next_absolute = True
        else:
            # Some argument contain the flag and the path together. For these
            # we parse the argument out of the path.
            for flag in FLAGS_PRECEDING_PATHS:
                if argument.startswith(flag):
                    argument = flag + os.path.join(build_path, argument[len(flag):])
                    break

        result.append(argument)
    return result


def create_make_trace_file(build_path):
    print "Creating make trace file for YouCompleteMe, might take a few seconds"
    os.chdir(build_path)
    subprocess.call("make -n -s > %s" % MAKE_TRACE_FILE_NAME, shell=True)


def make_trace_file_up_to_date(build_path):
    trace_file = os.path.join(build_path, MAKE_TRACE_FILE_NAME)
    if not os.path.isfile(trace_file):
        return False

    makefile = os.path.join(build_path, 'GNUmakefile')
    if os.path.getmtime(trace_file) < os.path.getctime(makefile):
        return False

    return True


def ensure_make_trace_file(build_path):
    if make_trace_file_up_to_date(build_path):
        return True
    create_make_trace_file(build_path)
    return make_trace_file_up_to_date(build_path)


def get_compilation_flags_from_build_commandline(command_line, build_path):
    flags = re.findall("-[I,D,p,O][\w,\.,/,-]*(?:=\"?\w*\"?)?", command_line)
    return transform_relative_paths_to_absolute_paths(flags, build_path)


def get_compilation_flags_for_file(build_path, filename):
    trace_file_path = os.path.join(build_path, MAKE_TRACE_FILE_NAME)

    try:
        trace_file = open(trace_file_path)
        lines = trace_file.readlines()
        trace_file.close()
    except:
        print "==== Error reading %s file", trace_file_path
        return []

    basename = os.path.basename(filename)
    for line in lines:
        if line.find(basename) != -1 and (line.find("CC") != -1 or line.find("CXX") != -1):
            return get_compilation_flags_from_build_commandline(line, build_path)

    print "Could not find flags for %s" % filename
    return []


def FlagsForFile(filename, **kwargs):
    """This is the main entry point for YCM. Its interface is fixed.

    Args:
      filename: (String) Path to source file being edited.

    Returns:
      (Dictionary)
        'flags': (List of Strings) Command line flags.
        'do_cache': (Boolean) True if the result should be cached.
    """

    result = {'flags': ['-std=c++11', '-x', 'c++'], 'do_cache': True}

    # Headers can't be built, so we get the source file flags instead.
    if filename.endswith('.h'):
        filename = filename[:-2] + ".cpp"
        if not os.path.exists(filename):
            return result

        # Force config.h file inclusion, for GLib macros.
        result['flags'].append("-includeconfig.h")

    build_path = common.get_build_path(fatal=False)
    if not build_path:
        print "Could not find WebKit build path."
        return result

    if not ensure_make_trace_file(build_path):
        print "Could not create make trace file"
        return result

    result['flags'].extend(get_compilation_flags_for_file(build_path, filename))
    return result

if __name__ == "__main__":
    import sys
    if len(sys.argv) >= 2:
        print FlagsForFile(sys.argv[1])

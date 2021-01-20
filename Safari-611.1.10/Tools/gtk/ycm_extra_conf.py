#!/usr/bin/env python
# Copyright (C) 2013 Danilo Cesar Lemes de Paula <danilo.eu@gmail.com>
# Copyright (C) 2014 ChangSeok Oh <shivamidow@gmail.com>
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
import sys
import ycm_core

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

top_level_directory = os.path.normpath(os.path.join(os.path.abspath(__tools_directory), '..', '..'))
sys.path.insert(0, os.path.join(top_level_directory, 'Tools', 'glib'))
import common


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


def get_build_path():
    webkitbuild_path = os.path.join(common.get_build_path(fatal=False), '..')
    if not os.path.exists(webkitbuild_path):
        return None

    release_build_path = os.path.join(webkitbuild_path, 'Release')
    debug_build_path = os.path.join(webkitbuild_path, 'Debug')

    try:
        release_mtime = os.path.getmtime(os.path.join(release_build_path, 'compile_commands.json'))
    except os.error:
        release_mtime = 0
    try:
        debug_mtime = os.path.getmtime(os.path.join(debug_build_path, 'compile_commands.json'))
    except os.error:
        debug_mtime = 0

    return release_build_path if release_mtime >= debug_mtime else debug_build_path


def getImplementationFilename(filename):
    alternative_extensions = ['.cpp', '.c']
    for alternative_extension in alternative_extensions:
        alternative_filename = filename[:-2] + alternative_extension
        if os.path.exists(alternative_filename):
            return alternative_filename
    return None


def FlagsForFile(filename, **kwargs):
    """This is the main entry point for YCM. Its interface is fixed.

    Args:
      filename: (String) Path to source file being edited.

    Returns:
      (Dictionary)
        'flags': (List of Strings) Command line flags.
        'do_cache': (Boolean) True if the result should be cached.
    """

    result = {'flags': ['-std=c++17', '-x', 'c++'], 'do_cache': True}

    # Headers can't be built, so we get the source file flags instead.
    if filename.endswith('.h'):
        implementationFilename = getImplementationFilename(filename)
        if implementationFilename:
            filename = implementationFilename
        else:
            if not filename.endswith('Inlines.h'):
                return result
            implementationFilename = getImplementationFilename(filename[:-len('Inlines.h')] + '.h')
            if not implementationFilename:
                return result
            filename = implementationFilename
        # Force config.h file inclusion, for GLib macros.
        result['flags'].append("-includeconfig.h")

    build_path = os.path.normpath(get_build_path())
    if not build_path:
        print("Could not find WebKit build path.")
        return result

    database = ycm_core.CompilationDatabase(build_path)
    if not database:
        print("Could not find compile_commands.json in %s, You might forget to add CMAKE_EXPORT_COMPILE_COMMANDS=ON to cmakeargs" % build_path)
        return result

    compilation_info = database.GetCompilationInfoForFile(filename)
    if not compilation_info:
        print("No compilation info.")
        return result

    result['flags'] = transform_relative_paths_to_absolute_paths(list(compilation_info.compiler_flags_), compilation_info.compiler_working_dir_)
    return result


if __name__ == "__main__":
    import sys
    if len(sys.argv) >= 2:
        print(FlagsForFile(sys.argv[1]))

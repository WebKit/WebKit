#!/usr/bin/env python3
#
# Copyright (c) 2014, 2015 Apple Inc. All rights reserved.
# Copyright (c) 2014 University of Washington. All rights reserved.
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

# This script generates C++ bindings for JavaScript builtins.
# Generators for individual files are located in the builtins/ directory.

import fnmatch
import logging
import optparse
import os
import re
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'wkbuiltins'))

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.ERROR)
log = logging.getLogger('global')

from lazywriter import LazyFileWriter

from wkbuiltins import *


def concatenated_output_filename(builtins_files, framework_name, generate_only_wrapper_files):
    if generate_only_wrapper_files:
        return framework_name + 'JSBuiltins.h-result'
    return os.path.basename(builtins_files[0]) + '-result'


def do_open(file, mode):
    return open(file, mode, encoding="UTF-8")


def security_macro_flag():
    SECURITY_RELEASE_MACRO = 'RC_REMOVE_SEC_ACCEL_3'
    # Configurations/SecurityReleaseMacros.xcconfig recognizes exactly the values 1 and YES.
    return ('-D' if os.environ.get(SECURITY_RELEASE_MACRO) in ('1', 'YES') else '-U') + SECURITY_RELEASE_MACRO


CONDITIONAL_DIRECTIVE_PATTERN = re.compile(r'^[ \t]*#[ \t]*(?:elif|else|endif|if|ifdef|ifndef)\b', re.MULTILINE)


def preprocess_builtins_file(filepath, file_text, macro_flag):
    if not CONDITIONAL_DIRECTIVE_PATTERN.search(file_text):
        return file_text

    if sys.platform != 'darwin':
        raise ParseException("No support for preprocessing of conditional directives in %s." % filepath)

    # -t keeps unifdef from lexing the JavaScript input as C.
    command = ['/usr/bin/xcrun', 'unifdef', '-t', macro_flag, filepath]
    result = subprocess.run(command, stdout=subprocess.PIPE, encoding="UTF-8")
    if result.returncode >= 2:
        raise ParseException("Failed to resolve conditional directives in %s." % filepath)

    return result.stdout


def generate_bindings_for_builtins_files(builtins_files=[],
                                         output_path=None,
                                         concatenate_output=False,
                                         combined_output=False,
                                         generate_only_wrapper_files=False,
                                         framework_name="",
                                         force_output=False):

    generators = []

    model = BuiltinsCollection(framework_name=framework_name)

    for filepath in builtins_files:
        with do_open(filepath, "r") as file:
            file_text = preprocess_builtins_file(filepath, file.read(), security_macro_flag())
            file_name = os.path.basename(filepath)

            # If this is a test file, then rewrite the filename to remove the
            # test running options encoded into the filename.
            if file_name.startswith(framework_name):
                (_, object_name, _) = file_name.split('-')
                file_name = object_name + '.js'
            model.parse_builtins_file(file_name, file_text)

    if combined_output:
        log.debug("Using generator style: combined files for all builtins.")
        generators.append(BuiltinsCombinedHeaderGenerator(model))
        generators.append(BuiltinsCombinedImplementationGenerator(model))
    else:
        log.debug("Using generator style: single files for each builtin.")
        if generate_only_wrapper_files:
            generators.append(BuiltinsWrapperHeaderGenerator(model))
            generators.append(BuiltinsWrapperImplementationGenerator(model))

            generators.append(BuiltinsInternalsWrapperHeaderGenerator(model))
            generators.append(BuiltinsInternalsWrapperImplementationGenerator(model))
        else:
            for object in model.objects:
                generators.append(BuiltinsSeparateHeaderGenerator(model, object))
                generators.append(BuiltinsSeparateImplementationGenerator(model, object))

    log.debug("")
    log.debug("Generating bindings for builtins.")

    test_result_file_contents = []

    for generator in generators:
        output_filepath = os.path.join(output_path, generator.output_filename())
        log.debug("Generating output file: %s" % generator.output_filename())
        output = generator.generate_output()

        log.debug("---")
        log.debug("\n" + output)
        log.debug("---")
        if concatenate_output:
            test_result_file_contents.append('### Begin File: %s' % generator.output_filename())
            test_result_file_contents.append(output)
            test_result_file_contents.append('### End File: %s' % generator.output_filename())
            test_result_file_contents.append('')
        else:
            log.debug("Writing file: %s" % output_filepath)
            output_file = LazyFileWriter(output_filepath, force_output)
            output_file.write(output)
            output_file.close()

    if concatenate_output:
        filename = concatenated_output_filename(builtins_files, framework_name, generate_only_wrapper_files)
        output_filepath = os.path.join(output_path, filename)
        log.debug("Writing file: %s" % output_filepath)
        output_file = LazyFileWriter(output_filepath, force_output)
        output_file.write('\n'.join(test_result_file_contents))
        output_file.close()

if __name__ == '__main__':
    allowed_framework_names = ['JavaScriptCore', 'WebCore']
    cli_parser = optparse.OptionParser(usage="usage: %prog [options] Builtin1.js [, Builtin2.js, ...]")
    cli_parser.add_option("-i", "--input-directory", help="If specified, generates builtins from all JavaScript files in the specified directory in addition to specific files passed as arguments.")
    cli_parser.add_option("-o", "--output-directory", help="Directory where generated files should be written.")
    cli_parser.add_option("--framework", type="choice", choices=allowed_framework_names, help="Destination framework for generated files.")
    cli_parser.add_option("--force", action="store_true", help="Force output of generated scripts, even if nothing changed.")
    cli_parser.add_option("--combined", action="store_true", help="Produce one .h/.cpp file instead of producing one per builtin object.")
    cli_parser.add_option("--wrappers-only", action="store_true", help="Produce .h/.cpp wrapper files to ease integration of the builtins.")
    cli_parser.add_option("-v", "--debug", action="store_true", help="Log extra output for debugging the generator itself.")
    cli_parser.add_option("-t", "--test", action="store_true", help="Enable test mode.")

    arg_options, arg_values = cli_parser.parse_args()
    if len(arg_values) == 0 and not arg_options.input_directory:
        raise ParseException("At least one input file or directory expected.")

    if not arg_options.output_directory:
        raise ParseException("Missing output directory.")

    if arg_options.debug:
        log.setLevel(logging.DEBUG)

    input_filepaths = arg_values[:]
    if arg_options.input_directory:
        for filepath in os.listdir(arg_options.input_directory):
            input_filepaths.append(os.path.join(arg_options.input_directory, filepath))

    input_filepaths = sorted([name for name in input_filepaths if fnmatch.fnmatch(name, '*.js')])

    options = {
        'output_path': arg_options.output_directory,
        'framework_name': arg_options.framework,
        'combined_output': arg_options.combined,
        'generate_only_wrapper_files': arg_options.wrappers_only,
        'force_output': arg_options.force,
        'concatenate_output': arg_options.test,
    }

    log.debug("Generating code for builtins.")
    log.debug("Parsed options:")
    for option, value in list(options.items()):
        log.debug("    %s: %s" % (option, value))
    log.debug("")
    log.debug("Input files:")
    for filepath in input_filepaths:
        log.debug("    %s" % filepath)
    log.debug("")

    try:
        generate_bindings_for_builtins_files(builtins_files=input_filepaths, **options)
    except ParseException as e:
        if arg_options.test:
            log.error(str(e))
        else:
            raise  # Force the build to fail.

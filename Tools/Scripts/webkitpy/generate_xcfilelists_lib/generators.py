#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2019 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# BaseGenerator is the base class for generating new .xcfilelist content. Most
# of the implementation is provided in this base class. Subclasses are created
# to provide project-specific information, such as the path to the project file
# and valid platform and configuration information.
#
# Instances of this class are create for each project/platform/configuration
# triple. For each triple, .xcfilelist content is generated and compared to the
# file that ultimately will contain that information. Any new lines are
# determined and remembered for later addition to that destination file.
#
# Instances of this class can operate in either of two contexts. First, it's
# possible for Xcode projects to invoke this script in order to keep their
# .xcfilelist files up-to-date. In that case, this script starts out running in
# the context of Xcode and it can just go ahead and generate the new content.
# Alternatively, it's possible for someone to invoke this script from the
# command line. In that case, this script needs to sublaunch itself in order to
# expose itself to the environment variables that Xcode establishes during
# builds. This script thus starts out as a free-standing script, creates a
# BaseGenerator for the .xcfilelist content it needs to create, sub-launches
# itself, and then creates another BaseGenerator to do the actual generation.
# When this inner instance of the script exits, it writes the results of the
# BaseGenerators to a temporary file. When the outer instance of the script
# resumes, it reads the information from that temporary file. That information
# is actually just a serialized (pickled) BaseGenerator instance and is
# restored as such. This restored BaseGenerator now replaces the original
# BaseGenerator in the outer script that caused it to be created, since the
# inner one is the one with all the xcfilelist information.

from __future__ import print_function

import os
import pickle
import re
import tempfile
import traceback

import webkitpy.generate_xcfilelists_lib.util as util
from webkitpy.xcode import xcode_hash_for_path
from webkitpy.xcode.sdk import SDK
from webkitpy.common.attribute_saver import AttributeSaver


class BaseGenerator(object):
    __slots__ = (
        "application", "project_tag", "platform", "configuration", "triple",
        "ex_type", "ex_value", "ex_traceback",
        "added_lines_input_derived", "added_lines_output_derived", "added_lines_input_unified", "added_lines_output_unified",
        "cached_build_dirs")

    VALID_PLATFORMS = None
    VALID_CONFIGURATIONS = None

    @util.LogEntryExit
    def __init__(self, application, project_tag, platform, configuration):
        self.application = application
        self.project_tag = project_tag
        self.platform = platform
        self.configuration = configuration
        self.triple = (project_tag, platform, configuration)

        self.ex_type = None
        self.ex_value = None
        self.ex_traceback = None

        self.added_lines_input_derived = None
        self.added_lines_output_derived = None
        self.added_lines_input_unified = None
        self.added_lines_output_unified = None

        self.cached_build_dirs = None

    def __str__(self):
        return "project_tag = {}, platform = {}, configuration = {}, ex_type = {}, ex_value = {}, ex_traceback = {}, added_lines_input_derived = {}, added_lines_output_derived = {}, added_lines_input_unified = {}, added_lines_output_unified = {}, cached_build_dirs = {}".format(
            self.project_tag, self.platform, self.configuration,
            self.ex_type, self.ex_value, self.ex_traceback,
            self.added_lines_input_derived, self.added_lines_output_derived, self.added_lines_input_unified, self.added_lines_output_unified,
            self.cached_build_dirs)

    # Generate new .xcfilist list contents and return any new lines. This
    # generation is performed by relaunching ourselves under Xcode so that we
    # can operate in the context of its environment variables. When we relaunch
    # ourselves, we do so in a way that invokes this class's generate()
    # method. Any results from generate are written to a temporary file that
    # we set up here. When the relaunched instance completes, we reads those
    # results from the temporary file.

    @util.LogEntryExit
    def set_environment_and_generate(self):
        with tempfile.NamedTemporaryFile() as pickle_file, tempfile.NamedTemporaryFile() as debug_file:
            sublaunch_args = [
                "PATH=\"{}:${{PATH}}\"".format(self._getenv("PATH")),  # Xcode will "sanitize" PATH, such that Python is no longer on it, so get /usr/bin back in PATH.
                "PYTHONPATH=\"{}\"".format(self._getenv("PYTHONPATH", "")),
                self.application.get_generate_xcfilelists_script_path(),
                "generate-inner",
                "--project", self.project_tag,
                "--platform", self.platform,
                "--configuration", self.configuration,
                "--pickle-file", pickle_file.name]
            if self.application.cmd_line_args.debug:
                sublaunch_args += [
                    "--debug",
                    "--debug-file", debug_file.name]
            if self.application.cmd_line_args.quiet:
                sublaunch_args.append("--quiet")

            try:
                self._sublaunch_under_xcode(sublaunch_args)
            finally:
                # If xcodebuild returns an error, we want to at least display the
                # debugging information.
                if self.application.cmd_line_args.debug:
                    for line in debug_file:
                        util.debug_log("{}".format(line.rstrip()))

            generators = []
            while True:
                try:
                    generator = pickle.load(pickle_file)
                    generator.application = self.application
                    generators.append(generator)
                except EOFError as e:
                    break
            return generators

    # Relaunch this script under Xcode. This is performed by launching Xcode,
    # pointing it to the project that we are processing as well as a build
    # target that will re-execute us as a custom build step.

    @util.LogEntryExit
    def _sublaunch_under_xcode(self, sublaunch_args):
        xcode_parameters = [
            "xcodebuild",
            "-project", self._get_project_file_path(),
            "-sdk", SDK.get_preferred_sdk_for_platform(self.platform).as_xcode_specification(),
            "-configuration", self.configuration,
            "-target", "Apply Configuration to XCFileLists",
            "SYMROOT={}".format(self._get_sym_root()),
            "OBJROOT={}".format(self._get_obj_root()),
            "SHARED_PRECOMPS_DIR={}".format(self._get_shared_precomps_dir())]

        # TODO: sublaunch_args will contain the path to the script to
        # sublaunch. There might be a problem if there's a space in that path.
        util.subprocess_run(xcode_parameters,
            env={"WK_SUBLAUNCH_SCRIPT_PARAMETERS": " ".join(sublaunch_args)})

    # Generate the .xcfilelist content. Save the results internally as sets of
    # new lines to be added to the .xcfilelist files. These new lines can be
    # merged into those files if the user specified a "generate" command, or
    # can be reported in a "check" command.

    @util.LogEntryExit
    def generate(self):
        self._generate_derived()
        self._generate_unified()

    # Merge any saved added lines to their ultimate destinations.

    @util.LogEntryExit
    def merge(self):
        self._merge_derived()
        self._merge_unified()

    @util.LogEntryExit
    def pickle_to_file(self, f):
        # We don't want to pickle the application reference
        # We can't seem to pickle ex_traceback: PicklingError: Can't pickle <type 'traceback'>: it's not found as __builtin__.traceback
        with AttributeSaver(self, "application"), AttributeSaver(self, "ex_traceback"):
            pickle.dump(self, f, pickle.HIGHEST_PROTOCOL)

    # Return whether or not any new lines for any .xcfilelist files were
    # discovered.

    @util.LogEntryExit
    def has_action(self):
        return (self.added_lines_input_derived or
                self.added_lines_output_derived or
                self.added_lines_input_unified or
                self.added_lines_output_unified)

    # If any errors occur during the generation of .xcfilelist content, they
    # are remembered internally for later processing. This function returns
    # whether or not any such error occurred with this generator.

    @util.LogEntryExit
    def has_error(self):
        return self.ex_type

    # Return whether or not the combination of project, platform, and
    # configuration is valid (which is to say, if, for example, the given
    # project can be built for the given platform and configuration).

    @util.LogEntryExit
    def is_valid(self):
        return (self.platform in self.__class__.VALID_PLATFORMS and
                self.configuration in self.__class__.VALID_CONFIGURATIONS)

    # If an error/exception occurred and was recorded (that is, if has_error()
    # returns true), report that error to the console.

    @util.LogEntryExit
    def report_error(self):
        try:
            if self.ex_value:
                raise self.ex_value
        except KeyboardInterrupt:
            print("### Canceled")
        except util.CalledProcessError as e:
            print("### Error {} calling subprocess: {}".format(e.args[0], e.args[1]))
            if e.args[2]:
                print("### stdout = {}".format(e.args[2]))
            if e.args[3]:
                print("### stderr = {}".format(e.args[3]))
        except util.InvalidConfigurationError as e:
            print("### Invalid configuration: {}".format(e))
            return os.EX_USAGE
        except BaseException:
            traceback.print_exception(self.ex_type, self.ex_value, self.ex_traceback)
            return os.EX_SOFTWARE

    # Generate .xcfilelist content for the "Generate Derived Sources" build
    # phase.

    @util.LogEntryExit
    def _generate_derived(self):
        script = self._get_generate_derived_sources_script()
        if not script:
            return

        with tempfile.NamedTemporaryFile(dir=self._get_temp_dir()) as input, \
                tempfile.NamedTemporaryFile(dir=self._get_temp_dir()) as output:
            (stdout, stderr) = util.subprocess_run(
                    [script,
                        "NO_SUPPLEMENTAL_FILES=1",
                        "--no-builtin-rules",
                        "--dry-run",
                        "--always-make",
                        "--debug=abvijm",
                        "all"])
            stdout = stdout.encode() if isinstance(stdout, str) else stdout
            (stdout, stderr) = util.subprocess_run(
                    [self.application.get_extract_dependencies_from_makefile_script(),
                        "--input", input.name,
                        "--output", output.name],
                    input=stdout)

            # TODO: Make this generator-specific (there's no need to reference
            # WebCore, for example, when processing the JavaScriptCore
            # project).

            input_lines = self._get_file_lines(input.name)
            output_lines = self._get_file_lines(output.name)

            input_lines = self._replace(input_lines, "^JavaScriptCore/",               "$(PROJECT_DIR)/")
            input_lines = self._replace(input_lines, "^JavaScriptCorePrivateHeaders/", "$(JAVASCRIPTCORE_PRIVATE_HEADERS_DIR)/")
            input_lines = self._replace(input_lines, "^WebCore/",                      "$(PROJECT_DIR)/")
            input_lines = self._replace(input_lines, "^WebKit2PrivateHeaders/",        "$(WEBKIT2_PRIVATE_HEADERS_DIR)/")

            input_lines = self._unexpand(input_lines, "JAVASCRIPTCORE_PRIVATE_HEADERS_DIR")
            input_lines = self._unexpand(input_lines, "PROJECT_DIR")
            input_lines = self._unexpand(input_lines, "WEBCORE_PRIVATE_HEADERS_DIR")
            input_lines = self._unexpand(input_lines, "WEBKIT2_PRIVATE_HEADERS_DIR")
            input_lines = self._unexpand(input_lines, "WEBKITADDITIONS_HEADERS_FOLDER_PATH")
            input_lines = self._unexpand(input_lines, "BUILT_PRODUCTS_DIR")    # Do this last, since it's a prefix of some other variables and will "intercept" them if executed earlier than them.

            output_lines = self._replace(output_lines, "^", self._get_derived_sources_dir() + "/")
            output_lines = self._unexpand(output_lines, "BUILT_PRODUCTS_DIR")

            self.added_lines_input_derived = self._find_added_lines(input_lines, self._get_input_derived_xcfilelist_project_path())
            self.added_lines_output_derived = self._find_added_lines(output_lines, self._get_output_derived_xcfilelist_project_path())

    @util.LogEntryExit
    def _merge_derived(self):
        self._merge_added_lines(self.added_lines_input_derived, self._get_input_derived_xcfilelist_project_path())
        self._merge_added_lines(self.added_lines_output_derived, self._get_output_derived_xcfilelist_project_path())

    # Generate .xcfilelist content for the "Generate Unified Sources" build
    # phase.

    @util.LogEntryExit
    def _generate_unified(self):
        script = self._get_generate_unified_sources_script()
        if not script:
            return

        with tempfile.NamedTemporaryFile(dir=self._get_temp_dir()) as output:

            # We need to define BUILD_SCRIPTS_DIR so that the bash script we're
            # invoking can find the ruby script that it invokes. If we don't
            # define BUILD_SCRIPTS_DIR, the bash script we're invoking with try
            # to define its own value for BUILD_SCRIPTS_DIR, but it will do so
            # incorrectly for our purposes, leading to dire results.

            env = os.environ.copy()
            env["BUILD_SCRIPTS_DIR"] = self.application.get_build_scripts_dir()

            util.subprocess_run(
                    [script,
                        "--generate-xcfilelists",
                        "--output-xcfilelist-path", output.name],
                    env=env)

            input_lines = None
            output_lines = self._get_file_lines(output.name)

            output_lines = self._unexpand(output_lines, "BUILT_PRODUCTS_DIR")

            self.added_lines_input_unified = self._find_added_lines(input_lines, self._get_input_unified_xcfilelist_project_path())
            self.added_lines_output_unified = self._find_added_lines(output_lines, self._get_output_unified_xcfilelist_project_path())

    @util.LogEntryExit
    def _merge_unified(self):
        self._merge_added_lines(self.added_lines_input_unified, self._get_input_unified_xcfilelist_project_path())
        self._merge_added_lines(self.added_lines_output_unified, self._get_output_unified_xcfilelist_project_path())

    # Utility for post-processing the initial .xcfilelist content. Used to
    # replace text in the file.

    @util.LogEntryExit
    def _replace(self, lines, to_replace, replace_with):
        return set([re.sub(to_replace, replace_with, line) for line in lines])

    # Utility for post-processing the initial .xcfilelist content. Used to
    # replace file path segments with the variables that represent those path
    # segments.

    @util.LogEntryExit
    def _unexpand(self, lines, variable_name):
        to_replace = self._getenv(variable_name)
        if not to_replace:
            return lines
        return self._replace(lines, "^{}/".format(to_replace), "$({})/".format(variable_name))

    # Given a source file with new .xcfilelist content and a dest file that
    # contains the original/previous .xcfilelist content (that is, likely the
    # file that's checked into the repo), determine what, if any, new lines
    # there are in source that aren't in dest.

    @util.LogEntryExit
    def _find_added_lines(self, source, dest):
        if not source:
            return set()

        def get_lines(source):
            return source if isinstance(source, set) else set(source) if isinstance(source, list) else self._get_file_lines(source)

        source_lines = get_lines(source)
        dest_lines = get_lines(dest)
        delta_lines = source_lines - dest_lines
        return delta_lines

    # Bottleneck routine for taking a set of new lines of .xcfilelist content
    # and adding them to their ultimate file/destination.

    @util.LogEntryExit
    def _merge_added_lines(self, added_lines, dest):
        if not added_lines:
            return
        dest_lines = self._get_file_lines(dest)
        merged_lines = sorted(set(added_lines) | dest_lines)
        merged_lines = [line + "\n" for line in merged_lines if line and not line.startswith("#")]
        merged_lines[0:0] = ["# This file is generated by the generate-xcfilelists script.\n"]
        with open(dest, "w") as f:
            f.writelines(merged_lines)

    # Utility to read a file and results the contents as a set of lines with
    # EOLs removed.

    @util.LogEntryExit
    def _get_file_lines(self, file):
        try:
            with open(file, "r") as f:
                return set([line.strip() for line in f])
        except:
            return set()

    # Wrapper to return environment variable values.

    @util.LogEntryExit
    def _getenv(self, variable_name, default=None):
        return os.environ.get(variable_name, default)

    # Return the path to the project file (the *.xcodeproj "file).

    @util.LogEntryExit
    def _get_project_file_path(self):
        assert False, """\
                Override this to return full path to the project file (e.g.,
                ".../Source/JavaScriptCore/JavaScriptCode.xcodeproj")."""

    # Return the path to the directory containing the project file and its
    # supporting files and directories (e.g., ".../Source/JavaScriptCore").

    @util.LogEntryExit
    def _get_project_dir(self):
        return os.path.dirname(self._get_project_file_path())

    # Return the project file name (e.g., "JavaScriptCore.xcodeproj").

    @util.LogEntryExit
    def _get_project_file_name(self):
        return os.path.basename(self._get_project_file_path())

    # Return the project name (e.g., "JavaScriptCore").

    @util.LogEntryExit
    def _get_project_name(self):
        return os.path.splitext(self._get_project_file_name())[0]

    # Return the path to the build output directory to use when the user has
    # indicated that they are building from the command line. Be sure to
    # support default command-line users, command-line users that set
    # WEBKIT_OUTPUTDIR, default Xcode users, and people like Jeff Miller who
    # configure a custom build output location for Xcode.

    @util.LogEntryExit
    def _get_sym_root(self):
        return self._get_build_dirs()[0]

    @util.LogEntryExit
    def _get_obj_root(self):
        return self._get_build_dirs()[1]

    @util.LogEntryExit
    def _get_shared_precomps_dir(self):
        return os.path.join(self._get_build_dirs()[1], "PrecompiledHeaders")

    # From Xcode;
    #
    #   export SYMROOT            =/Users/keith/Library/Developer/Xcode/DerivedData/Safari-bmjhivzkbpxamlajyexvkivfjbmb/Build/Products
    #   export OBJROOT            =/Users/keith/Library/Developer/Xcode/DerivedData/Safari-bmjhivzkbpxamlajyexvkivfjbmb/Build/Intermediates.noindex
    #   export SHARED_PRECOMPS_DIR=/Users/keith/Library/Developer/Xcode/DerivedData/Safari-bmjhivzkbpxamlajyexvkivfjbmb/Build/Intermediates.noindex/PrecompiledHeaders
    #
    # From command-line:
    #
    #   export SYMROOT            =/Volumes/Data/dev/webkit/OpenSource/WebKitBuild
    #   export OBJROOT            =/Volumes/Data/dev/webkit/OpenSource/WebKitBuild
    #   export SHARED_PRECOMPS_DIR=/Volumes/Data/dev/webkit/OpenSource/WebKitBuild/PrecompiledHeaders

    @util.LogEntryExit
    def _get_build_dirs(self):
        def define_xcode_build_dirs(self):
            # Delete any spurious ~/Library/Preferences/xcodebuild.plist, as this
            # file will interfere with any preferences set in the IDE. This
            # .plist file shouldn't really ever exist, so nuking it doesn't
            # cause any problems.

            xcodebuild_plist = os.path.join(os.path.expanduser("~"), "Library", "Preferences", "xcodebuild.plist")
            try:
                os.unlink(xcodebuild_plist)
            except:
                pass

            def read_xcode_user_default(key):
                try:
                    (stdout, stderr) = util.subprocess_run(["defaults", "read", "com.apple.dt.Xcode", key])
                    return stdout.strip()
                except util.CalledProcessError:
                    return None

            # The following is based on the logic in determineBaseProductDir()
            # in webkitdirs.pm and https://pewpewthespells.com/blog/xcode_build_locations.html.

            # Get the base directory for the build output. This will be some
            # default location (e.g., ~/Library/Developer/Xcode/DerivedData),
            # an absolute path, or a project-relative path.

            ide_custom_derived_data_location = read_xcode_user_default("IDECustomDerivedDataLocation")

            # Path not specified; use the default.

            if not ide_custom_derived_data_location:
                base_dir = os.path.join(os.path.expanduser("~"), "Library", "Developer", "Xcode", "DerivedData")

            # An absolute path is specified; use it.

            elif os.path.isabs(ide_custom_derived_data_location):
                base_dir = ide_custom_derived_data_location

            # A relative path is specified; append it to the project path.

            else:
                base_dir = os.path.join(self._get_project_dir(), ide_custom_derived_data_location)

            # Get the specification for how the build output should be stored
            # withing that base directory. This will be some unique directory
            # based on the hash of the project file path (e.g.,
            # "Safari-sdlfkhasalksdjfhsdfhlksf"), some shared directory (e.g.,
            # "Build"), or even something that might be an absolute path.

            ide_build_location_style = read_xcode_user_default("IDEBuildLocationStyle")

            # Create a unique directory within the base directory based on
            # project name and hash of its full path.
            #
            #    IDEBuildLocationStyle      = Unique;       # This is the default if not specified.

            if ide_build_location_style == "Unique" or not ide_build_location_style:
                workspace = os.path.abspath(self.application.cmd_line_args.xcode)
                build_dir = os.path.join(
                        base_dir,
                        os.path.splitext(os.path.basename(workspace))[0] + "-" + xcode_hash_for_path(workspace),
                        "Build")
                products_dir = os.path.join(build_dir, "Products")
                intermediates_dir = os.path.join(build_dir, "Intermediates.noindex")

            # Use a shared subdirectory; use the specified directory name.
            #
            #    IDEBuildLocationStyle      = Shared;
            #    IDESharedBuildFolderName       = Build;    # Relative to DerivedDataLocation

            elif ide_build_location_style == "Shared":
                build_dir = os.path.join(base_dir, read_xcode_user_default("IDESharedBuildFolderName"))
                products_dir = os.path.join(build_dir, "Products")
                intermediates_dir = os.path.join(build_dir, "Intermediates.noindex")

            # Use the saved products and intermediates paths and either use
            # them as relative to the DerivedData directory or the project
            # folder, or just use them as absolute addresses.
            #
            #    IDEBuildLocationStyle      = Custom;

            elif ide_build_location_style == "Custom":
                ide_build_location_type = read_xcode_user_default("IDECustomBuildLocationType")

                products_dir = read_xcode_user_default("IDECustomBuildProductsPath")
                intermediates_dir = read_xcode_user_default("IDECustomBuildIntermediatesPath")

                #    IDECustomBuildLocationType     = RelativeToDerivedData;
                #    IDECustomBuildIntermediatesPath    = "Build/Intermediates.noindex";
                #    IDECustomBuildProductsPath         = "Build/Products";
                #    IDECustomIndexStorePath            = "Index/DataStore";

                if ide_build_location_type == "RelativeToDerivedData":
                    products_dir = os.path.join(base_dir, products_dir)
                    intermediates_dir = os.path.join(base_dir, intermediates_dir)

                #    IDECustomBuildLocationType     = RelativeToWorkspace;
                #    IDECustomBuildIntermediatesPath    = "Build/Intermediates.noindex";
                #    IDECustomBuildProductsPath         = "Build/Products";
                #    IDECustomIndexStorePath            = "Index/DataStore";

                elif ide_build_location_type == "RelativeToWorkspace":
                    base_dir = self._get_project_dir(),
                    products_dir = os.path.join(base_dir, products_dir)
                    intermediates_dir = os.path.join(base_dir, intermediates_dir)

                #    IDECustomBuildLocationType     = Absolute;
                #    IDECustomBuildIntermediatesPath    = "/Users/joedeveloper/Desktop/Build/Intermediates.noindex";
                #    IDECustomBuildProductsPath         = "/Users/joedeveloper/Desktop/Build/Products";
                #    IDECustomIndexStorePath            = "/Users/joedeveloper/Desktop/Index/DataStore";
                #    IDECustomDerivedDataLocation       = "/Users/joedeveloper/Library/Developer/Xcode/DerivedData";

                elif ide_build_location_type == "Absolute":
                    pass

                else:
                    assert False, "Unknown/unsupported location type"
            else:
                assert False, "Unknown/unsupported style"

            return (products_dir, intermediates_dir)

        def define_command_line_build_dirs(self):
            products_dir = self._getenv("WEBKIT_OUTPUTDIR")
            if not products_dir:
                products_dir = os.path.join(self.application.get_opensource_dir(), "WebKitBuild")
            return (products_dir, products_dir)

        if not self.cached_build_dirs and self.application.cmd_line_args.xcode:
            self.cached_build_dirs = define_xcode_build_dirs(self)
        if not self.cached_build_dirs:
            self.cached_build_dirs = define_command_line_build_dirs(self)
        return self.cached_build_dirs

    # Return the location of the DerivedSources directory. Conventionally, we
    # use $BUILT_PRODUCTS/DerivedSources/$PROJECT_NAME, but we may some day use
    # $DERIVED_FILE_DIR aka $DERIVED_FILES_DIR aka $DERIVED_SOURCES_DIR, when
    # the projects have been updated to use them.

    @util.LogEntryExit
    def _get_derived_sources_dir(self):
        return os.path.join(self.application.get_xcode_built_products_dir(), "DerivedSources", self._get_project_name())

    # Return the location for the xcfilelists.

    @util.LogEntryExit
    def _get_xcfilelist_dir(self):
        return self._get_project_dir()

    # Return the paths to the actual xcfilelists.

    @util.LogEntryExit
    def _get_input_derived_xcfilelist_project_path(self):
        return os.path.join(self._get_xcfilelist_dir(), "DerivedSources-input.xcfilelist")

    @util.LogEntryExit
    def _get_output_derived_xcfilelist_project_path(self):
        return os.path.join(self._get_xcfilelist_dir(), "DerivedSources-output.xcfilelist")

    @util.LogEntryExit
    def _get_input_unified_xcfilelist_project_path(self):
        return os.path.join(self._get_xcfilelist_dir(), "UnifiedSources-input.xcfilelist")

    @util.LogEntryExit
    def _get_output_unified_xcfilelist_project_path(self):
        return os.path.join(self._get_xcfilelist_dir(), "UnifiedSources-output.xcfilelist")

    # Return the paths to the scripts that generate the derived sources.

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return None

    @util.LogEntryExit
    def _get_generate_unified_sources_script(self):
        return None

    @util.LogEntryExit
    def _get_temp_dir(self):
        return self.application.get_xcode_project_temp_dir()


class JavaScriptCoreGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", "iphoneos", "iphonesimulator", "watchos", "watchsimulator", "appletvos", "appletvsimulator")
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production", "Profiling")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Source", "JavaScriptCore", "JavaScriptCore.xcodeproj")

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-derived-sources.sh")

    @util.LogEntryExit
    def _get_generate_unified_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-unified-sources.sh")


class WebCoreGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", "iphoneos", "iphonesimulator", "watchos", "watchsimulator", "appletvos", "appletvsimulator")
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Source", "WebCore", "WebCore.xcodeproj")

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-derived-sources.sh")


class WebKitGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", "iphoneos", "iphonesimulator", "watchos", "watchsimulator", "appletvos", "appletvsimulator")
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Source", "WebKit", "WebKit.xcodeproj")

    @util.LogEntryExit
    def _get_derived_sources_dir(self):
        return os.path.join(self.application.get_xcode_built_products_dir(), "DerivedSources", "WebKit2")

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-derived-sources.sh")

    @util.LogEntryExit
    def _get_generate_unified_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-unified-sources.sh")


class WebKitLegacyGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", "iphoneos", "iphonesimulator", "watchos", "watchsimulator", "appletvos", "appletvsimulator")
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Source", "WebKitLegacy", "WebKitLegacy.xcodeproj")

    @util.LogEntryExit
    def _get_generate_unified_sources_script(self):
        return os.path.join(self._get_project_dir(), "scripts", "generate-unified-sources.sh")


class DumpRenderTreeGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", )
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Tools", "DumpRenderTree", "DumpRenderTree.xcodeproj")

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-derived-sources.sh")


class WebKitTestRunnerGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", )
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Tools", "WebKitTestRunner", "WebKitTestRunner.xcodeproj")

    @util.LogEntryExit
    def _get_generate_derived_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-derived-sources.sh")


class TestWebKitAPIGenerator(BaseGenerator):
    VALID_PLATFORMS = ("macosx", "iphoneos", "iphonesimulator", "watchos", "watchsimulator", "appletvos", "appletvsimulator")
    VALID_CONFIGURATIONS = ("Debug", "Release", "Production")

    @util.LogEntryExit
    def _get_project_file_path(self):
        return os.path.join(self.application.get_opensource_dir(), "Tools", "TestWebKitAPI", "TestWebKitAPI.xcodeproj")

    @util.LogEntryExit
    def _get_generate_unified_sources_script(self):
        return os.path.join(self._get_project_dir(), "Scripts", "generate-unified-sources.sh")

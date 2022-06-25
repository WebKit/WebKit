#!/usr/bin/env ruby

# Copyright (C) 2015 Apple Inc. All rights reserved.
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

require 'fileutils'
require 'tmpdir'

if ARGV.size != 1 || ARGV[0].include?("-h")
  puts "usage: #{File.basename $0} <output-directory>"
  exit 1
end

JAVASCRIPTCORE_PATH = File.expand_path File.join(File.dirname(__FILE__), "..", "..", "JavaScriptCore")
WEB_INSPECTOR_PATH = File.expand_path File.join(File.dirname(__FILE__), "..")
COPY_USER_INTERFACE_RESOURCES_PATH = File.join WEB_INSPECTOR_PATH, "Scripts", "copy-user-interface-resources.pl"

# This script simulates processing user interface resources located in SRCROOT.
# It places processed files in the specified output directory. This is most similar
# to an isolated OBJROOT since it includes DerivedSources. It doesn't place files
# into their DSTROOT locations, such as inside of WebInspectorUI.framework.
$output_directory = File.expand_path ARGV[0]
$start_directory = FileUtils.pwd

Dir.mktmpdir do |tmpdir|

  # Create the output directory if needed.
  FileUtils.mkdir_p $output_directory
  
  # Create empty derived sources expected to exist.
  FileUtils.touch(File.join(tmpdir, 'InspectorBackendCommands.js'))
  
  # Setup the environment and run.
  ENV["DERIVED_SOURCES_DIR"] = tmpdir
  # Stage some scripts expected to be in various framework PrivateHeaders.
  ENV["JAVASCRIPTCORE_PRIVATE_HEADERS_DIR"] = tmpdir
  FileUtils.cp(File.join(JAVASCRIPTCORE_PATH, "Scripts", "cssmin.py"), tmpdir)
  FileUtils.cp(File.join(JAVASCRIPTCORE_PATH, "Scripts", "jsmin.py"), tmpdir)
  ENV["SRCROOT"] = WEB_INSPECTOR_PATH
  ENV["TARGET_BUILD_DIR"] = $output_directory
  ENV["UNLOCALIZED_RESOURCES_FOLDER_PATH"] = ""
  ENV["COMBINE_INSPECTOR_RESOURCES"] = "YES"
  ENV["COMBINE_TEST_RESOURCES"] = "YES"
  ENV["FORCE_TOOL_INSTALL"] = "NO"
  FileUtils.cd $start_directory
  system(COPY_USER_INTERFACE_RESOURCES_PATH) or raise "Failed to process user interface resources."

end

#!/usr/bin/env ruby
#
# Copyright (c) 2017-2023 Apple Inc. All rights reserved.
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

require "fileutils"
require 'erb'
require 'optparse'

require 'preference_lib'

options = {
  :frontend => nil,
  :outputDirectory => Dir.getwd,
  :templates => [],
  :preferenceFiles => []
}
optparse = OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename($0)} --frontend <frontend> [--outputDir <output>] --template <file> [--template <file>...] <preferences> [<preferences>...]"

  opts.separator ""

  opts.on("--frontend input", "frontend to generate preferences for (WebKit, WebKitLegacy)") { |frontend| options[:frontend] = frontend }
  opts.on("--template input", "template to use for generation (may be specified multiple times)") { |template| options[:templates] << template }
  opts.on("--outputDir output", "directory to generate file in (default: cwd)") { |outputDir| options[:outputDirectory] = outputDir }
  opts.on("-h", "--help", "show this help message") { puts opts; exit 1 }
end

optparse.parse!

options[:preferenceFiles] = ARGV.shift(ARGV.size)
if options[:preferenceFiles].empty?
  puts optparse
  exit 1
end

FileUtils.mkdir_p(options[:outputDirectory])

preferences = Preferences.new(options[:preferenceFiles], options[:frontend])

options[:templates].each do |template|
  preferences.renderTemplate(template, options[:outputDirectory])
end

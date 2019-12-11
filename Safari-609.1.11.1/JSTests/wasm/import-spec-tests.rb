#!/usr/bin/env ruby

# Copyright (C) 2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

require 'fileutils'
require 'getoptlong'
require 'pathname'
require 'rbconfig'
require 'open3'

THIS_SCRIPT_PATH = Pathname.new(__FILE__).realpath
WASM_PATH = THIS_SCRIPT_PATH.dirname
raise unless WASM_PATH.basename.to_s == "wasm"
raise unless WASM_PATH.dirname.basename.to_s == "JSTests"

def usage
    puts ""
    puts "usage:"
    puts "    import-spec-tests.rb --spec <path-to-wasm-spec-git-repo> [-v]"
    puts ""
    puts "    the wasm spec's git repo can be found here: https://github.com/WebAssembly/spec"
    puts ""
    exit 1
end

$specDirectory = nil
$verbose = false

GetoptLong.new(['--spec',GetoptLong::REQUIRED_ARGUMENT],
               ['-v', GetoptLong::OPTIONAL_ARGUMENT],
               ['--help', GetoptLong::OPTIONAL_ARGUMENT],
               ).each {
    | opt, arg |
    case opt
    when '--help'
        usage
    when '--spec'
        $specDirectory = arg
    when '-v'
        $verbose = true
    end
}

raise unless $specDirectory

$resultDirectory = File.join(WASM_PATH, "spec-tests")
$harnessDirectory = File.join(WASM_PATH, "spec-harness")

$specTestDirectory = File.join($specDirectory, "test")

def removeDir(file)
    begin
        FileUtils.remove_dir(file)
    rescue
        puts "No directory: #{file}" if $verbose
    end
end

removeDir($resultDirectory)
removeDir($harnessDirectory)

FileUtils.mkdir($resultDirectory)
FileUtils.cp_r(File.join($specTestDirectory, "harness"), $harnessDirectory)

$genScript = File.join($specTestDirectory, "build.py")
stdout, stderr, status = Open3.capture3("#{$genScript} --js #{$resultDirectory}")
if stderr != ""
    puts "failed to generate tests"
    puts "The error is:\n--------------\n #{stderr}\n--------------\n" if $verbose
end
puts stdout if $verbose


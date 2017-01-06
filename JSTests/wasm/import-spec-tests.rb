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
    puts "    import-spec-tests.rb --spec <path-to-wasm-spec-git-repo> --wabt <path-to-wabt-git-repo> [-v]"
    puts ""
    puts "    wabt's git repo can be found here: https://github.com/WebAssembly/wabt"
    puts "    the wasm spec's git repo can be found here: https://github.com/WebAssembly/spec"
    puts ""
    exit 1
end

$specDirectory = nil
$wabtDirectory = nil
$verbose = false

GetoptLong.new(['--spec',GetoptLong::REQUIRED_ARGUMENT],
               ['--wabt', GetoptLong::REQUIRED_ARGUMENT],
               ['-v', GetoptLong::OPTIONAL_ARGUMENT],
               ['--help', GetoptLong::OPTIONAL_ARGUMENT],
               ).each {
    | opt, arg |
    case opt
    when '--help'
        usage
    when '--spec'
        $specDirectory = arg
    when '--wabt'
        $wabtDirectory = arg
    when '-v'
        $verbose = true
    end
}

raise unless $specDirectory
raise unless $wabtDirectory

$resultDirectory = File.join(WASM_PATH, "spec-tests")

begin
    FileUtils.remove_dir($resultDirectory)
rescue
    puts "No directroy: #{$resultDirectory}" if $verbose
end

FileUtils.mkdir($resultDirectory)

$wabtScript = File.join($wabtDirectory, "test", "run-gen-spec-js.py")

Dir.entries(File.join($specDirectory, "interpreter", "test")).each {
    | wast |

    next if File.extname(wast) != ".wast"

    stdout, stderr, status = Open3.capture3("#{$wabtScript} #{File.join($specDirectory, "interpreter", "test", wast)}")
    if stderr != ""
        puts "Skipping making test for file: #{wast} because of a wabt error"
        puts "The error is:\n--------------\n #{stderr}\n--------------\n" if $verbose
    else
        resultFile = wast + ".js"
        puts "Creating imported JS file: #{File.join($resultDirectory, resultFile)}"
        File.open(File.join($resultDirectory, resultFile), "w") { 
            |f| f.write(stdout)
        }
    end
}

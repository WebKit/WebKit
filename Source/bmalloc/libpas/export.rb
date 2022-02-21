#!/usr/bin/env ruby
# Copyright (c) 2021 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
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

opts = GetoptLong.new(
    [ '--help', '-h', GetoptLong::NO_ARGUMENT ],
    [ '--force', '-f', GetoptLong::NO_ARGUMENT ]
)

$force = false

opts.each {
    | opt, arg |
    case opt
    when '--help'
        puts "Usage: ./export.rb [-f] <destination>"
        puts
        puts "Copies local git checkout (including unstaged changes to files in the repo) to the"
        puts "destination. Must be run in the repo's directory (the directory that contains this"
        puts "script). Does not change the timestamp of files that did not change."
        puts
        puts "Options:"
        puts "--help (-h)     Print this message."
        puts "--force (-f)    Copy to the destination even if it doesn't look like a libpas directory."
        exit 0

    when '--force'
        $force = true

    else
        raise
    end
}

raise "Bad argument; see --help" unless ARGV.length == 1

$destination = Pathname.new(ARGV[0]).realpath

unless Pathname.new("src/libpas").directory?
    puts "Error: must run this script from a libpas directory."
    exit 1
end

unless ($destination + "src/libpas").directory?
    if $force
        puts "Warning: destination does not look like a libpas directory."
    else
        puts "Error: destination does not look like a libpas directory; use --force to override."
        exit 1
    end
end

def prepareDirectory(directory)
    return if directory.directory?
    raise if directory.to_s == "."
    raise if directory.to_s.length <= $destination.to_s.length
    prepareDirectory(directory.dirname)
    puts "Preparing directory #{directory}"
    FileUtils.rm_rf(directory)
    directory.mkdir
end

IO.popen("git ls-files", "r") {
    | inp |
    inp.each_line {
        | line |
        line.chomp!
        inputFile = Pathname.new(line)

        raise if inputFile.directory?
        
        outputFile = $destination + line

        contents = inputFile.read

        if outputFile.exist? and
          inputFile.directory? == outputFile.directory? and
          inputFile.executable? == outputFile.executable? and
          contents == outputFile.read
            #puts "Skipping #{line}"
            next
        end

        puts "Copying #{line} to #{outputFile}"

        prepareDirectory(outputFile.dirname)

        FileUtils.rm_rf(outputFile)
        outputFile.write(contents)

        outputFile.chmod(0755) if inputFile.executable?
    }
}

raise unless $? == 0



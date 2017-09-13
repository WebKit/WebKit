# Copyright (C) 2017 Apple Inc. All rights reserved.
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
require 'pathname'
require 'getoptlong'

SCRIPT_NAME = File.basename($0)

def usage
    puts "usage: #{SCRIPT_NAME} [options] -p <desination-path> <source-file> [<source-file>...]"
    puts "--help                          (-h) Print this message"
    puts "--derived-sources-path          (-p) Path to the directory where the unified source files should be placed. This argument is required."
    puts "--verbose                       (-v) Adds extra logging to stderr."
    exit 1
end

MAX_BUNDLE_SIZE = 8
$derivedSourcesPath = nil
$verbose = false
# FIXME: Use these when Xcode uses unified sources.
$maxCppBundleCount = 100000
$maxObjCBundleCount = 100000

GetoptLong.new(['--help', '-h', GetoptLong::NO_ARGUMENT],
               ['--derived-sources-path', '-p', GetoptLong::REQUIRED_ARGUMENT],
               ['--verbose', '-v', GetoptLong::NO_ARGUMENT],
               ['--max-cpp-bundle-count', GetoptLong::REQUIRED_ARGUMENT],
               ['--max-obj-c-bundle-count', GetoptLong::REQUIRED_ARGUMENT]).each {
    | opt, arg |
    case opt
    when '--help'
        usage
    when '--derived-sources-path'
        $derivedSourcesPath = Pathname.new(arg)
    when '--verbose'
        $verbose = true
    when '--max-cpp-bundle-count'
        $maxCppBundleCount = arg
    when '--max-obj-c-bundle-count'
        $maxObjCBundleCount = arg
    end
}

usage if !$derivedSourcesPath || ARGV.empty?

def log(text)
    $stderr.puts text if $verbose
end

$generatedSources = []

class BundleManager
    attr_reader :bundleCount, :extension, :fileCount, :currentBundleText

    def initialize(extension)
        @extension = extension
        @fileCount = 0
        @bundleCount = 0
        @currentBundleText = ""
    end

    def flush
        # No point in writing an empty bundle file
        return if @currentBundleText == ""

        @bundleCount += 1
        bundleFile = "UnifiedSource#{@bundleCount}#{extension}"
        bundleFile = $derivedSourcesPath + bundleFile
        log("writing bundle #{bundleFile} with: \n#{@currentBundleText}")
        IO::write(bundleFile, @currentBundleText)
        $generatedSources << bundleFile

        @currentBundleText = ""
        @fileCount = 0
    end

    def addFile(file)
        raise "wrong extension: #{file.extname} expected #{@extension}" unless file.extname == @extension
        if @fileCount == MAX_BUNDLE_SIZE
            log("flushing because new bundle is full #{@fileCount}")
            flush
        end
        @currentBundleText += "#include \"#{file}\"\n"
        @fileCount += 1
    end
end

$bundleManagers = {
    ".cpp" => BundleManager.new(".cpp"),
    ".mm" => BundleManager.new(".mm")
}

$currentDirectory = nil


ARGV.sort.each {
    | file |

    path = Pathname.new(file)
    if ($currentDirectory != path.dirname)
        log("flushing because new dirname old: #{$currentDirectory}, new: #{path.dirname}")
        $bundleManagers.each_value { | x | x.flush }
        $currentDirectory = path.dirname
    end

    bundle = $bundleManagers[path.extname]
    if !bundle
        log("No bundle for #{path.extname} files building #{path} standalone")
        $generatedSources << path
    else
        bundle.addFile(path)
    end
}

$bundleManagers.each_value { | x | x.flush }

# We use stdout to report our unified source list to CMake.
# Add trailing semicolon since CMake seems dislikes not having it.
# Also, make sure we use print instead of puts because CMake will think the \n is a source file and fail to build.
print($generatedSources.join(";") + ";")

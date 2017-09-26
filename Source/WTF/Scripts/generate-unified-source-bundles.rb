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
COMMENT_REGEXP = /#/

def usage
    puts "usage: #{SCRIPT_NAME} [options] <sources-file>"
    puts "--help                          (-h) Print this message"
    puts "--verbose                       (-v) Adds extra logging to stderr."
    puts "Required arguments:"
    puts "--source-tree-path              (-s) Path to the root of the source directory."
    puts "--derived-sources-path          (-d) Path to the directory where the unified source files should be placed."
    puts
    puts "Optional arguments:"
    puts "--print-bundled-sources              Print bundled sources rather than generating sources"
    puts
    puts "Generation options:"
    puts "--max-cpp-bundle-count               Sets the limit on the number of cpp bundles that can be generated"
    puts "--max-obj-c-bundle-count             Sets the limit on the number of Obj-C bundles that can be generated"
    exit 1
end

MAX_BUNDLE_SIZE = 8
$derivedSourcesPath = nil
$unifiedSourceOutputPath = nil
$sourceTreePath = nil
$verbose = false
$mode = :GenerateBundles
$maxCppBundleCount = 100000
$maxObjCBundleCount = 100000

def log(text)
    $stderr.puts text if $verbose
end

GetoptLong.new(['--help', '-h', GetoptLong::NO_ARGUMENT],
               ['--verbose', '-v', GetoptLong::NO_ARGUMENT],
               ['--derived-sources-path', '-d', GetoptLong::REQUIRED_ARGUMENT],
               ['--source-tree-path', '-s', GetoptLong::REQUIRED_ARGUMENT],
               ['--print-bundled-sources', GetoptLong::NO_ARGUMENT],
               ['--max-cpp-bundle-count', GetoptLong::REQUIRED_ARGUMENT],
               ['--max-obj-c-bundle-count', GetoptLong::REQUIRED_ARGUMENT]).each {
    | opt, arg |
    case opt
    when '--help'
        usage
    when '--verbose'
        $verbose = true
    when '--derived-sources-path'
        $derivedSourcesPath = Pathname.new(arg)
        $unifiedSourceOutputPath = $derivedSourcesPath + Pathname.new("unified-sources")
        FileUtils.mkdir($unifiedSourceOutputPath) if !$unifiedSourceOutputPath.exist?
    when '--source-tree-path'
        $sourceTreePath = Pathname.new(arg)
        usage if !$sourceTreePath.exist?
    when '--print-bundled-sources'
        $mode = :PrintBundledSources
    when '--max-cpp-bundle-count'
        $maxCppBundleCount = arg.to_i
    when '--max-obj-c-bundle-count'
        $maxObjCBundleCount = arg.to_i
    end
}

usage if !$unifiedSourceOutputPath || !$sourceTreePath
log("putting unified sources in #{$unifiedSourceOutputPath}")

usage if ARGV.length == 0
$generatedSources = []

class SourceFile < Pathname
    attr_reader :unifiable
    def initialize(file)
        @unifiable = true

        attributeStart = file =~ COMMENT_REGEXP
        if attributeStart
            # attributes start with @ so we want skip the comment character and the first @.
            attributesText = file[(attributeStart + 2)..file.length]
            attributesText.split(/\s*@/).each {
                | attribute |
                case attribute
                when "no-unify"
                    @unifiable = false
                end
            }
            file = file.split(" ")[0]
        end

        super(file)
    end

    def derived?
        return @derived if @derived != nil
        @derived = !($sourceTreePath + self).exist?
    end

    def display
        if $mode == :GenerateBundles || !derived?
            self.to_s
        else
            ($derivedSourcesPath + self).to_s
        end
    end
end

class BundleManager
    attr_reader :bundleCount, :extension, :fileCount, :currentBundleText, :maxCount

    def initialize(extension, max)
        @extension = extension
        @fileCount = 0
        @bundleCount = 0
        @currentBundleText = ""
        @maxCount = max
    end

    def bundleFileName(number)
        "UnifiedSource#{number}.#{extension}"
    end

    def flush
        # No point in writing an empty bundle file
        return if @currentBundleText == ""

        @bundleCount += 1
        bundleFile = $unifiedSourceOutputPath + bundleFileName(@bundleCount)
        $generatedSources << bundleFile

        if (!bundleFile.exist? || IO::read(bundleFile) != @currentBundleText)
            log("writing bundle #{bundleFile} with: \n#{@currentBundleText}")
            IO::write(bundleFile, @currentBundleText)
        end

        @currentBundleText = ""
        @fileCount = 0
    end

    def addFile(file)
        raise "wrong extension: #{file.extname} expected #{@extension}" unless file.extname == ".#{@extension}"
        if @fileCount == MAX_BUNDLE_SIZE
            log("flushing because new bundle is full #{@fileCount}")
            flush
        end
        @currentBundleText += "#include \"#{file}\"\n"
        @fileCount += 1
    end
end

def ProcessFileForUnifiedSourceGeneration(path)
    if ($currentDirectory != path.dirname)
        log("flushing because new dirname old: #{$currentDirectory}, new: #{path.dirname}")
        $bundleManagers.each_value { |x| x.flush }
        $currentDirectory = path.dirname
    end

    bundle = $bundleManagers[path.extname]
    if !bundle || !path.unifiable
        log("No bundle for #{path.extname} files building #{path} standalone")
        $generatedSources << path
    else
        bundle.addFile(path)
    end
end

$bundleManagers = {
    ".cpp" => BundleManager.new("cpp", $maxCppBundleCount),
    ".mm" => BundleManager.new("mm", $maxObjCBundleCount)
}

ARGV.each {
    | sourcesFile |
    log("reading #{sourcesFile}")
    sources = File.read(sourcesFile).split($/).keep_if {
        | line |
        # Only strip lines if they start with a comment since sources we don't
        # want to bundle have an attribute, which starts with a comment.
        !((line =~ COMMENT_REGEXP) == 0 || line.empty?)
    }

    log("found #{sources.length} source files in #{sourcesFile}")

    sources.sort.each {
        | file |

        path = SourceFile.new(file)
        case $mode
        when :GenerateBundles
            ProcessFileForUnifiedSourceGeneration(path)
        when :PrintBundledSources
            $generatedSources << path if $bundleManagers[path.extname] && path.unifiable
        end
    }

    $bundleManagers.each_value { |x| x.flush } if $mode == :GenerateBundles
}

$bundleManagers.each_value {
    | manager |

    maxCount = manager.maxCount
    bundleCount = manager.bundleCount
    extension = manager.extension
    if bundleCount > maxCount
        filesToAdd = ((maxCount+1)..bundleCount).map { |x| manager.bundleFileName(x) }.join(", ")
        raise "number of bundles for #{extension} sources, #{bundleCount}, exceeded limit, #{maxCount}. Please add #{filesToAdd} to Xcode then update UnifiedSource#{extension.capitalize}FileCount"
    end
}

# We use stdout to report our unified source list to CMake.
# Add trailing semicolon since CMake seems dislikes not having it.
# Also, make sure we use print instead of puts because CMake will think the \n is a source file and fail to build.

$generatedSources.map! { |path| path.display } if $mode == :PrintBundledSources
print($generatedSources.join(";") + ";")

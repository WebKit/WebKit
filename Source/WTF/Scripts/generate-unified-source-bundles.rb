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
COMMENT_REGEXP = /\/\//

def usage(message)
    if message
        puts "Error: #{message}"
        puts
    end

    puts "usage: #{SCRIPT_NAME} [options] <sources-list-file>..."
    puts "<sources-list-file> may be separate arguments or one semicolon separated string"
    puts "--help                          (-h) Print this message"
    puts "--verbose                       (-v) Adds extra logging to stderr."
    puts "Required arguments:"
    puts "--source-tree-path              (-s) Path to the root of the source directory."
    puts "--derived-sources-path          (-d) Path to the directory where the unified source files should be placed."
    puts
    puts "Optional arguments:"
    puts "--print-bundled-sources              Print bundled sources rather than generating sources"
    puts "--feature-flags                 (-f) Space or semicolon separated list of enabled feature flags"
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
$featureFlags = {}
$verbose = false
$mode = :GenerateBundles
$maxCppBundleCount = nil
$maxObjCBundleCount = nil

def log(text)
    $stderr.puts text if $verbose
end

GetoptLong.new(['--help', '-h', GetoptLong::NO_ARGUMENT],
               ['--verbose', '-v', GetoptLong::NO_ARGUMENT],
               ['--derived-sources-path', '-d', GetoptLong::REQUIRED_ARGUMENT],
               ['--source-tree-path', '-s', GetoptLong::REQUIRED_ARGUMENT],
               ['--feature-flags', '-f', GetoptLong::REQUIRED_ARGUMENT],
               ['--print-bundled-sources', GetoptLong::NO_ARGUMENT],
               ['--max-cpp-bundle-count', GetoptLong::REQUIRED_ARGUMENT],
               ['--max-obj-c-bundle-count', GetoptLong::REQUIRED_ARGUMENT]).each {
    | opt, arg |
    case opt
    when '--help'
        usage(nil)
    when '--verbose'
        $verbose = true
    when '--derived-sources-path'
        $derivedSourcesPath = Pathname.new(arg)
        $unifiedSourceOutputPath = $derivedSourcesPath + Pathname.new("unified-sources")
        FileUtils.mkpath($unifiedSourceOutputPath) if !$unifiedSourceOutputPath.exist?
    when '--source-tree-path'
        $sourceTreePath = Pathname.new(arg)
        usage("Source tree #{arg} does not exist.") if !$sourceTreePath.exist?
    when '--feature-flags'
        arg.gsub(/\s+/, ";").split(";").map { |x| $featureFlags[x] = true }
    when '--print-bundled-sources'
        $mode = :PrintBundledSources
    when '--max-cpp-bundle-count'
        $maxCppBundleCount = arg.to_i
    when '--max-obj-c-bundle-count'
        $maxObjCBundleCount = arg.to_i
    end
}

usage("--derived-sources-path must be specified.") if !$unifiedSourceOutputPath
usage("--source-tree-path must be specified.") if !$sourceTreePath
log("Putting unified sources in #{$unifiedSourceOutputPath}")
log("Active Feature flags: #{$featureFlags.keys.inspect}")

usage("At least one source list file must be specified.") if ARGV.length == 0
# Even though CMake will only pass us a single semicolon separated arguemnts, we separate all the arguments for simplicity.
sourceListFiles = ARGV.to_a.map { | sourceFileList | sourceFileList.split(";") }.flatten
log("Source files: #{sourceListFiles}")
$generatedSources = []

class SourceFile
    attr_reader :unifiable, :fileIndex, :path
    def initialize(file, fileIndex)
        @unifiable = true
        @fileIndex = fileIndex

        attributeStart = file =~ /@/
        if attributeStart
            # We want to make sure we skip the first @ so split works correctly
            attributesText = file[(attributeStart + 1)..file.length]
            attributesText.split(/\s*@/).each {
                | attribute |
                case attribute.strip
                when "no-unify"
                    @unifiable = false
                else
                    raise "unknown attribute: #{attribute}"
                end
            }
            file = file[0..(attributeStart-1)]
        end

        @path = Pathname.new(file.strip)
    end

    def <=>(other)
        return @path.dirname <=> other.path.dirname if @path.dirname != other.path.dirname
        return @path.basename <=> other.path.basename if @fileIndex == other.fileIndex
        @fileIndex <=> other.fileIndex
    end

    def derived?
        return @derived if @derived != nil
        @derived = !($sourceTreePath + self.path).exist?
    end

    def to_s
        if $mode == :GenerateBundles || !derived?
            @path.to_s
        else
            ($derivedSourcesPath + @path).to_s
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

    def writeFile(file, text)
        bundleFile = $unifiedSourceOutputPath + file
        if (!bundleFile.exist? || IO::read(bundleFile) != @currentBundleText)
            log("Writing bundle #{bundleFile} with: \n#{@currentBundleText}")
            IO::write(bundleFile, @currentBundleText)
        end
    end

    def bundleFileName(number)
        @extension == "cpp" ? "UnifiedSource#{number}.#{extension}" : "UnifiedSource#{number}-#{extension}.#{extension}"
    end

    def flush
        # No point in writing an empty bundle file
        return if @currentBundleText == ""

        @bundleCount += 1
        bundleFile = bundleFileName(@bundleCount)
        $generatedSources << $unifiedSourceOutputPath + bundleFile

        writeFile(bundleFile, @currentBundleText)
        @currentBundleText = ""
        @fileCount = 0
    end

    def flushToMax
        raise if !@maxCount
        ((@bundleCount+1)..@maxCount).each {
            | index |
            writeFile(bundleFileName(index), "")
        }
    end

    def addFile(sourceFile)
        path = sourceFile.path
        raise "wrong extension: #{path.extname} expected #{@extension}" unless path.extname == ".#{@extension}"
        if @fileCount == MAX_BUNDLE_SIZE
            log("Flushing because new bundle is full (#{@fileCount} sources)")
            flush
        end
        @currentBundleText += "#include \"#{sourceFile}\"\n"
        @fileCount += 1
    end
end

def TopLevelDirectoryForPath(path)
    if !path
        return nil
    end
    while path.dirname != path.dirname.dirname
        path = path.dirname
    end
    return path
end

def ProcessFileForUnifiedSourceGeneration(sourceFile)
    path = sourceFile.path
    if (TopLevelDirectoryForPath($currentDirectory) != TopLevelDirectoryForPath(path.dirname))
        log("Flushing because new top level directory; old: #{$currentDirectory}, new: #{path.dirname}")
        $bundleManagers.each_value { |x| x.flush }
        $currentDirectory = path.dirname
    end

    bundle = $bundleManagers[path.extname]
    if !bundle
        log("No bundle for #{path.extname} files, building #{path} standalone")
        $generatedSources << sourceFile
    elsif !sourceFile.unifiable
        log("Not allowed to unify #{path}, building standalone")
        $generatedSources << sourceFile
    else
        bundle.addFile(sourceFile)
    end
end

$bundleManagers = {
    ".cpp" => BundleManager.new("cpp", $maxCppBundleCount),
    ".mm" => BundleManager.new("mm", $maxObjCBundleCount)
}

seen = {}
sourceFiles = []

sourceListFiles.each_with_index {
    | path, sourceFileIndex |
    log("Reading #{path}")
    result = []
    inDisabledLines = false
    File.read(path).lines.each {
        | line |
        commentStart = line =~ COMMENT_REGEXP
        log("Before: #{line}")
        if commentStart != nil
            line = line.slice(0, commentStart)
            log("After: #{line}")
        end
        line.strip!
        if line == "#endif"
            inDisabledLines = false
            next
        end

        next if line.empty? || inDisabledLines

        if line =~ /\A#if/
            raise "malformed #if" unless line =~ /\A#if\s+(\S+)/
            inDisabledLines = !$featureFlags[$1]
        else
            raise "malformed preprocessor directive: #{line}" if line =~ /^#/
            raise "duplicate line: #{line} in #{path}" if seen[line]
            seen[line] = true
            result << SourceFile.new(line, sourceFileIndex)
        end
    }
    raise "Couldn't find closing \"#endif\"" if inDisabledLines

    log("Found #{result.length} source files in #{path}")
    sourceFiles += result
}

log("Found sources: #{sourceFiles.sort}")

sourceFiles.sort.each {
    | sourceFile |
    case $mode
    when :GenerateBundles
        ProcessFileForUnifiedSourceGeneration(sourceFile)
    when :PrintBundledSources
        $generatedSources << sourceFile if $bundleManagers[sourceFile.path.extname] && sourceFile.unifiable
    end
}

$bundleManagers.each_value {
    | manager |
    manager.flush

    maxCount = manager.maxCount
    next if !maxCount

    manager.flushToMax
    bundleCount = manager.bundleCount
    extension = manager.extension
    if bundleCount > maxCount
        filesToAdd = ((maxCount+1)..bundleCount).map { |x| manager.bundleFileName(x) }.join(", ")
        raise "number of bundles for #{extension} sources, #{bundleCount}, exceeded limit, #{maxCount}. Please add #{filesToAdd} to Xcode then update UnifiedSource#{extension.capitalize}FileCount"
    end
}

# We use stdout to report our unified source list to CMake.
# Add trailing semicolon and avoid a trailing newline for CMake's sake.

log($generatedSources.join(";") + ";")
print($generatedSources.join(";") + ";")

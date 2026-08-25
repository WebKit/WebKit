#!/usr/bin/env ruby
#
# Copyright (c) 2026 Apple Inc. All rights reserved.
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

# Generates the SecurityFlags structure from SecurityFlags.yaml. Kept apart from GeneratePreferences.rb so that
# this schema cannot drift into accepting a regular preference's fields.

require "fileutils"
require 'erb'
require 'optparse'
require 'yaml'

options = {
  :outputDirectory => Dir.getwd,
  :templates => [],
  :securityFlagFiles => []
}
optparse = OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename($0)} [--outputDir <output>] --template <file> [--template <file>...] <securityFlags> [<securityFlags>...]"

  opts.separator ""

  opts.on("--template input", "template to use for generation (may be specified multiple times)") { |template| options[:templates] << template }
  opts.on("--outputDir output", "directory to generate file in (default: cwd)") { |outputDir| options[:outputDirectory] = outputDir }
  opts.on("-h", "--help", "show this help message") { puts opts; exit 1 }
end

optparse.parse!

options[:securityFlagFiles] = ARGV.shift(ARGV.size)
if options[:securityFlagFiles].empty?
  puts optparse
  exit 1
end

FileUtils.mkdir_p(options[:outputDirectory])

# Keys as the file spells them, in order and including repeats. Validation cannot use the hash YAML.load_file
# returns, because YAML silently collapses a repeated key and keeps only the last one.
def keysInFileOrder(path)
  document = begin
    YAML.parse_file(path)
  rescue Psych::SyntaxError => e
    STDERR.puts "error: Could not parse input file #{path}: #{e.message}"
    exit(1)
  end
  return [] if !document

  root = document.children[0]
  return [] if !root

  if !root.is_a?(Psych::Nodes::Mapping)
    STDERR.puts "error: Input file #{path} is not a mapping of security flag names to their fields."
    exit(1)
  end

  root.children.each_slice(2).map { |key, _| key.value }
end

# Keys sort by magnitude: a plain string sort would put radar1000000000 before radar99999999. Nil for a name
# that is not a radar number, so the sort check can leave reporting that to the check built for it.
def radarNumber(name)
  digits = name[/\Aradar([1-9][0-9]*)\z/, 1]
  digits && digits.to_i
end

def load(path)
  seen = {}
  previousName = nil

  keysInFileOrder(path).each do |name|
    if seen[name]
      STDERR.puts "error: Input file #{path} defines '#{name}' more than once. Only the last one would survive, silently discarding the other entry."
      exit(1)
    end
    seen[name] = true

    previousNumber = previousName && radarNumber(previousName)
    number = radarNumber(name)
    if previousNumber and number and previousNumber > number
      STDERR.puts "error: Input file #{path} is not sorted by radar number. First out of order name found is '#{name}'."
      exit(1)
    end
    previousName = name
  end

  begin
    YAML.load_file(path)
  rescue ArgumentError => e
    STDERR.puts "error: Could not parse input file: #{e.message}"
    exit(1)
  end
end

class SecurityFlag
  attr_accessor :name
  attr_accessor :humanReadableDescription
  attr_accessor :condition

  # Index into the generated BitSet, assigned from the sorted key order once every file has been read.
  attr_accessor :index

  def initialize(name, opts)
    @name = name
    @humanReadableDescription = opts["humanReadableDescription"]
    @condition = opts["condition"]
  end
end

class SecurityFlags
  attr_accessor :securityFlags

  # Every field a flag may carry; anything else is an error.
  FIELDS = %w{ humanReadableDescription condition }

  def initialize(securityFlagFiles)
    @securityFlags = []
    @fileForName = {}
    securityFlagFiles.each do |file|
      initializeParsedSecurityFlags(load(file), file)
    end

    @securityFlags.sort_by! { |flag| radarNumber(flag.name) }
    @securityFlags.each_with_index { |flag, index| flag.index = index }

    @warning = "THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT."
  end

  def initializeParsedSecurityFlags(parsedSecurityFlags, path)
    failed = false
    reject = Proc.new do |msg|
      STDERR.puts("error: " + msg)
      failed = true
    end

    if parsedSecurityFlags
      parsedSecurityFlags.each do |name, options|
        options ||= {}

        reject.call "Security flag name '#{name}' is not a radar number, which must be spelled \"radar\" followed by the number with no leading zero, e.g. radar184485266." if !(name =~ /\Aradar[1-9][0-9]*\z/)

        if @fileForName[name]
          reject.call "Security flag #{name} is defined in both #{@fileForName[name]} and #{path}."
        else
          @fileForName[name] = path
        end

        (options.keys - FIELDS).each do |field|
          reject.call "Security flag #{name} has \"#{field}\", which SecurityFlags.yaml does not support. Only #{FIELDS.join(", ")} are allowed: the type is always bool, the default is always the secure value, and these flags never surface in a features UI or reach WebCore."
        end

        description = options["humanReadableDescription"]
        if !description
          reject.call "Security flag #{name} has no humanReadableDescription, which is required: the key is only a radar number, so this is the only record of what the flag guards when we later decide whether it can be retired."
        elsif !description.is_a?(String) or description.empty?
          reject.call "Security flag #{name} has a humanReadableDescription that is not a non-empty string."
        end

        condition = options["condition"]
        reject.call "Security flag #{name} has a condition that is not a string." if condition and !condition.is_a?(String)

        @securityFlags << SecurityFlag.new(name, options) if !failed
      end
    end
    exit 1 if failed
  end

  def createTemplate(templateString)
    # Newer versions of ruby deprecate and/or drop passing non-keyword
    # arguments for trim_mode and friends, so we need to call the constructor
    # differently depending on what it expects. This solution is suggested by
    # rubocop's Lint/ErbNewArguments.
    if ERB.instance_method(:initialize).parameters.assoc(:key) # Ruby 2.6+
      ERB.new(templateString, trim_mode:"-")
    else
      ERB.new(templateString, nil, "-")
    end
  end

  def renderTemplate(templateFile, outputDirectory)
    resultFile = File.join(outputDirectory, File.basename(templateFile, ".erb"))
    tempResultFile = resultFile + ".tmp"

    erb = createTemplate(File.read(templateFile))
    erb.filename = templateFile
    output = erb.result(binding)
    File.open(tempResultFile, "w+") do |f|
      f.write(output)
    end
    if (!File.exist?(resultFile) || IO::read(resultFile) != IO::read(tempResultFile))
      FileUtils.move(tempResultFile, resultFile)
    else
      FileUtils.remove_file(tempResultFile)
      FileUtils.uptodate?(resultFile, [templateFile]) or FileUtils.touch(resultFile)
    end
  end
end

securityFlags = SecurityFlags.new(options[:securityFlagFiles])

options[:templates].each do |template|
  securityFlags.renderTemplate(template, options[:outputDirectory])
end

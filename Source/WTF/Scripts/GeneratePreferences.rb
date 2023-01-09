#!/usr/bin/env ruby
#
# Copyright (c) 2017, 2020 Apple Inc. All rights reserved.
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
require 'yaml'

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

options[:preferenceFiles] = ARGV.slice!(0...)
if options[:preferenceFiles].empty?
  puts optparse
  exit 1
end

FileUtils.mkdir_p(options[:outputDirectory])

def load(path)
  parsed = begin
    YAML.load_file(path)
  rescue ArgumentError => e
    STDERR.puts "error: Could not parse input file: #{e.message}"
    exit(1)
  end
  if parsed
    previousName = nil
    parsed.keys.each do |name|
      if previousName != nil and previousName > name
        STDERR.puts "error: Input file #{path} is not sorted. First out of order name found is '#{name}'."
        exit(1)
      end
      previousName = name
    end
  end
  parsed
end

class Preference
  attr_accessor :name
  attr_accessor :opts
  attr_accessor :type
  attr_accessor :refinedType
  attr_accessor :status
  attr_accessor :humanReadableName
  attr_accessor :humanReadableDescription
  attr_accessor :webcoreBinding
  attr_accessor :condition
  attr_accessor :hidden
  attr_accessor :defaultValues
  attr_accessor :exposed

  def initialize(name, opts, frontend)
    @name = name
    @opts = opts
    @type = opts["type"]
    @refinedType = opts["refinedType"]
    @status = opts["status"]
    @humanReadableName = (opts["humanReadableName"] || "")
    if not humanReadableName.start_with? "WebKitAdditions"
        @humanReadableName = '"' + humanReadableName + '"'
    end
    @humanReadableDescription = (opts["humanReadableDescription"] || "")
    if not humanReadableDescription.start_with? "WebKitAdditions"
        @humanReadableDescription = '"' + humanReadableDescription + '"'
    end
    @getter = opts["getter"]
    @webcoreBinding = opts["webcoreBinding"]
    @webcoreName = opts["webcoreName"]
    @condition = opts["condition"]
    @hidden = opts["hidden"] || false
    @defaultValues = opts["defaultValue"][frontend]
    @exposed = !opts["exposed"] || opts["exposed"].include?(frontend)
  end

  def nameLower
    if @getter
      @getter
    elsif @name.start_with?("VP")
      @name[0..1].downcase + @name[2..@name.length]
    elsif @name.start_with?("CSS", "DOM", "DNS", "FTP", "ICE", "IPC", "PDF", "XSS")
      @name[0..2].downcase + @name[3..@name.length]
    elsif @name.start_with?("HTTP")
      @name[0..3].downcase + @name[4..@name.length]
    else
      @name[0].downcase + @name[1..@name.length]
    end
  end

  def webcoreNameUpper
    if @webcoreName
      @webcoreName[0].upcase + @webcoreName[1..@webcoreName.length]
    else
      @name
    end
  end

  def typeUpper
    if @type == "uint32_t"
      "UInt32"
    else
      @type.capitalize
    end
  end

  def webFeatureStatus
    "WebFeatureStatus" + @status.capitalize
  end

  def apiStatus
    "API::FeatureStatus::" + @status.capitalize
  end

  # WebKitLegacy specific helpers.

  def preferenceKey
    if @opts["webKitLegacyPreferenceKey"]
      @opts["webKitLegacyPreferenceKey"]
    else
      "WebKit#{@name}"
    end
  end

  def preferenceAccessor
    case @type
    when "bool"
      "_boolValueForKey"
    when "uint32_t"
      "_integerValueForKey"
    when "double"
     "_floatValueForKey"
    when "String"
      "_stringValueForKey"
    else
      raise "Unknown type: #{@type}"
    end
  end

  def downcast
    if @refinedType
      "static_cast<#{@type}>("
    end
  end

  def upcast
    if @refinedType
      "static_cast<#{@refinedType}>("
    end
  end

  def ephemeral?
    %w{ embedder unstable internal testable }.include? @status
  end

  def defaultOverridable?
    %w{ internal }.include? @status
  end

  # FIXME: These names correspond to the "experimental features" and "internal
  # debug features" designations used before the feature status taxonomy was
  # introduced. They should be renamed to better reflect their semantic meanings.

  # Features which should appear in UI presented to end users.
  def experimental?
    %w{ developer testable preview stable }.include? @status
  end

  # Features which should only be presented in WebKit development contexts.
  def internal?
    %w{ unstable internal }.include? @status
  end
  
  # Features which should be enabled while running tests
  def testable?
    %w{ testable preview stable }.include? @status
  end
end

class Preferences
  attr_accessor :preferences

  def initialize(preferenceFiles, frontend)
    @frontend = frontend

    @preferences = []
    preferenceFiles.each do |file|
      initializeParsedPreferences(load(file))
    end

    @preferences.sort_by! { |p| p.humanReadableName.empty? ? p.name : p.humanReadableName }
    @exposedPreferences = @preferences.select { |p| p.exposed }

    @preferencesBoundToSetting = @preferences.select { |p| !p.webcoreBinding }
    @preferencesBoundToDeprecatedGlobalSettings = @preferences.select { |p| p.webcoreBinding == "DeprecatedGlobalSettings" }

    @warning = "THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT."
  end

  # Corresponds to WebFeatureStatus enum cases. "developer" and up require human-readable names.
  STATUSES = %w{ embedder unstable internal developer testable preview stable }

  def initializeParsedPreferences(parsedPreferences)
    result = []
    failed = false
    reject = Proc.new do |msg|
      STDERR.puts("error: " + msg)
      failed = true
    end

    if parsedPreferences
      parsedPreferences.each do |name, options|
        webcoreSettingOnly = !options["webcoreBinding"] && options["defaultValue"].keys == ["WebCore"]
        status = options["status"]
        if !STATUSES.include?(status)
          reject.call "Preference #{name}'s status \"#{status}\" is not one of the known statuses: #{STATUSES}"
          next
        end

        if %w{ unstable internal developer testable preview stable }.include?(status)
          reject.call "Preference #{name} has no humanReadableName, which is required." if !options["humanReadableName"]
          reject.call "Preference #{name} is visible in client UI and has a default value bound to WebCore::Settings, so it must have default values for all frontends" if webcoreSettingOnly
          next if failed
        elsif webcoreSettingOnly and @frontend != "WebCore"
          next
        end

        if options["defaultValue"].include?(@frontend)
          preference = Preference.new(name, options, @frontend)
          @preferences << preference
          result << preference
        end
      end
    end
    exit 1 if failed
    result
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

preferences = Preferences.new(options[:preferenceFiles], options[:frontend])

options[:templates].each do |template|
  preferences.renderTemplate(template, options[:outputDirectory])
end

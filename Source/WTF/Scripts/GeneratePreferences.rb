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
  :basePreferences => nil,
  :debugPreferences => nil,
  :experimentalPreferences => nil,
  :internalPreferences => nil,
  :outputDirectory => nil,
  :templates => []
}
optparse = OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename($0)} --input file"

  opts.separator ""

  opts.on("--frontend input", "frontend to generate preferences for (WebKit, WebKitLegacy)") { |frontend| options[:frontend] = frontend }
  opts.on("--base input", "file to generate preferences from") { |basePreferences| options[:basePreferences] = basePreferences }
  opts.on("--debug input", "file to generate debug preferences from") { |debugPreferences| options[:debugPreferences] = debugPreferences }
  opts.on("--experimental input", "file to generate experimental preferences from") { |experimentalPreferences| options[:experimentalPreferences] = experimentalPreferences }
  opts.on("--internal input", "file to generate internal preferences from") { |internalPreferences| options[:internalPreferences] = internalPreferences }
  opts.on("--template input", "template to use for generation (may be specified multiple times)") { |template| options[:templates] << template }
  opts.on("--outputDir output", "directory to generate file in") { |outputDir| options[:outputDirectory] = outputDir }
end

optparse.parse!

if !options[:frontend] || !options[:basePreferences] || !options[:debugPreferences] || !options[:experimentalPreferences] || !options[:internalPreferences]
  puts optparse
  exit -1
end

if !options[:outputDirectory]
  options[:outputDirectory] = Dir.getwd
end

FileUtils.mkdir_p(options[:outputDirectory])

def load(path)
  parsed = begin
    YAML.load_file(path)
  rescue ArgumentError => e
    puts "ERROR: Could not parse input file: #{e.message}"
    exit(-1)
  end
  if parsed
    previousName = nil
    parsed.keys.each do |name|
      if previousName != nil and previousName > name
        puts "ERROR: Input file #{path} is not sorted. First out of order name found is '#{name}'."
        exit(-1)
      end
      previousName = name
    end
  end
  parsed
end

parsedBasePreferences = load(options[:basePreferences])
parsedDebugPreferences = load(options[:debugPreferences])
parsedExperimentalPreferences = load(options[:experimentalPreferences])
parsedInternalPreferences = load(options[:internalPreferences])


class Preference
  attr_accessor :name
  attr_accessor :opts
  attr_accessor :type
  attr_accessor :humanReadableName
  attr_accessor :humanReadableDescription
  attr_accessor :webcoreBinding
  attr_accessor :condition
  attr_accessor :hidden
  attr_accessor :defaultValues

  def initialize(name, opts, frontend)
    @name = name
    @opts = opts
    @type = opts["type"]
    @humanReadableName = '"' + (opts["humanReadableName"] || "") + '"'
    @humanReadableDescription = '"' + (opts["humanReadableDescription"] || "") + '"'
    @getter = opts["getter"]
    @webcoreBinding = opts["webcoreBinding"]
    @webcoreName = opts["webcoreName"]
    @condition = opts["condition"]
    @hidden = opts["hidden"] || false
    @defaultValues = opts["defaultValue"][frontend]
  end

  def nameLower
    if @getter
      @getter
    elsif @name.start_with?("VP")
      @name[0..1].downcase + @name[2..@name.length]
    elsif @name.start_with?("CSS", "XSS", "FTP", "DOM", "DNS", "PDF", "ICE")
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
end

class Preferences
  attr_accessor :preferences

  def initialize(parsedBasePreferences, parsedDebugPreferences, parsedExperimentalPreferences, parsedInternalPreferences, frontend)
    @frontend = frontend

    @preferences = []
    @preferencesNotDebug = initializeParsedPreferences(parsedBasePreferences)
    @preferencesDebug = initializeParsedPreferences(parsedDebugPreferences)
    @experimentalFeatures = initializeParsedPreferences(parsedExperimentalPreferences)
    @internalFeatures = initializeParsedPreferences(parsedInternalPreferences)

    @preferences.sort! { |x, y| x.name <=> y.name }
    @preferencesNotDebug.sort! { |x, y| x.name <=> y.name }
    @preferencesDebug.sort! { |x, y| x.name <=> y.name }
    @experimentalFeatures.sort! { |x, y| x.name <=> y.name }.sort! { |x, y| x.humanReadableName <=> y.humanReadableName }
    @internalFeatures.sort! { |x, y| x.name <=> y.name }.sort! { |x, y| x.humanReadableName <=> y.humanReadableName }

    @preferencesBoundToSetting = @preferences.select { |p| !p.webcoreBinding }
    @preferencesBoundToDeprecatedGlobalSettings = @preferences.select { |p| p.webcoreBinding == "DeprecatedGlobalSettings" }
    @preferencesBoundToRuntimeEnabledFeatures = @preferences.select { |p| p.webcoreBinding == "RuntimeEnabledFeatures" }

    @warning = "THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT."
  end

  def initializeParsedPreferences(parsedPreferences)
    result = []
    if parsedPreferences
      parsedPreferences.each do |name, options|
        if !options["exposed"] or options["exposed"].include?(@frontend)
          preference = Preference.new(name, options, @frontend)
          @preferences << preference
          result << preference
        end
      end
    end
    result
  end

  def renderTemplate(templateFile, outputDirectory)
    puts "Generating output for template file: #{templateFile}"

    resultFile = File.basename(templateFile, ".erb")

    output = ERB.new(File.read(templateFile), 0, "-").result(binding)
    File.open(File.join(outputDirectory, resultFile), "w+") do |f|
      f.write(output)
    end
  end
end

preferences = Preferences.new(parsedBasePreferences, parsedDebugPreferences, parsedExperimentalPreferences, parsedInternalPreferences, options[:frontend])

options[:templates].each do |template|
  preferences.renderTemplate(template, options[:outputDirectory])
end

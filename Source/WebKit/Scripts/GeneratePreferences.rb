#!/usr/bin/env ruby
#
# Copyright (c) 2017 Apple Inc. All rights reserved.
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
  :input => nil,
  :outputDirectory => nil
}
optparse = OptionParser.new do |opts|
    opts.banner = "Usage: #{File.basename($0)} --input file"

    opts.separator ""

    opts.on("--input input", "file to generate settings from") { |input| options[:input] = input }
    opts.on("--outputDir output", "directory to generate file in") { |output| options[:outputDirectory] = output }
end

optparse.parse!

if !options[:input]
  puts optparse
  exit -1
end

if !options[:outputDirectory]
  options[:outputDirectory] = Dir.getwd
end

FileUtils.mkdir_p(options[:outputDirectory])

parsedPreferences = begin
  YAML.load_file(options[:input])
rescue ArgumentError => e
  puts "Could not parse input file: #{e.message}"
  exit(-1)
end

class Preference
  attr_accessor :name
  attr_accessor :type
  attr_accessor :defaultValue
  attr_accessor :humanReadableName
  attr_accessor :humanReadableDescription
  attr_accessor :category
  attr_accessor :webcoreBinding
  attr_accessor :condition

  def initialize(name, opts)
    @name = name
    @type = opts["type"]
    @defaultValue = opts["defaultValue"]
    @humanReadableName = '"' + (opts["humanReadableName"] || "") + '"'
    @humanReadableDescription = '"' + (opts["humanReadableDescription"] || "") + '"'
    @category = opts["category"]
    @getter = opts["getter"]
    @webcoreBinding = opts["webcoreBinding"]
    @webcoreName = opts["webcoreName"]
    @condition = opts["condition"]
  end

  def nameLower
    if @getter
      @getter
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

end

class Conditional
  attr_accessor :condition
  attr_accessor :preferences

  def initialize(condition, settings)
    @condition = condition
    @preferences = preferences
  end
end

class Preferences
  attr_accessor :preferences
  
  def initialize(hash)
    @preferences = []
    hash.each do |name, options|
      @preferences << Preference.new(name, options)
    end
    @preferences.sort! { |x, y| x.name <=> y.name }
    
    @preferencesNotDebug = @preferences.select { |p| !p.category }
    @preferencesDebug = @preferences.select { |p| p.category == "debug" }
    @experimentalFeatures = @preferences.select { |p| p.category == "experimental" }

    @preferencesBoundToSetting = @preferences.select { |p| !p.webcoreBinding }
    @preferencesBoundToDeprecatedGlobalSettings = @preferences.select { |p| p.webcoreBinding == "DeprecatedGlobalSettings" }
    @preferencesBoundToRuntimeEnabledFeatures = @preferences.select { |p| p.webcoreBinding == "RuntimeEnabledFeatures" }
  end

  def renderToFile(template, file)
    template = File.join(File.dirname(__FILE__), template)

    output = ERB.new(File.read(template), 0, "-").result(binding)
    File.open(file, "w+") do |f|
      f.write(output)
    end
  end
end

preferences = Preferences.new(parsedPreferences)
preferences.renderToFile("PreferencesTemplates/WebPreferencesDefinitions.h.erb", File.join(options[:outputDirectory], "WebPreferencesDefinitions.h"))
preferences.renderToFile("PreferencesTemplates/WebPageUpdatePreferences.cpp.erb", File.join(options[:outputDirectory], "WebPageUpdatePreferences.cpp"))
preferences.renderToFile("PreferencesTemplates/WebPreferencesKeys.h.erb", File.join(options[:outputDirectory], "WebPreferencesKeys.h"))
preferences.renderToFile("PreferencesTemplates/WebPreferencesKeys.cpp.erb", File.join(options[:outputDirectory], "WebPreferencesKeys.cpp"))
preferences.renderToFile("PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb", File.join(options[:outputDirectory], "WebPreferencesStoreDefaultsMap.cpp"))
preferences.renderToFile("PreferencesTemplates/WebPreferencesExperimentalFeatures.cpp.erb", File.join(options[:outputDirectory], "WebPreferencesExperimentalFeatures.cpp"))

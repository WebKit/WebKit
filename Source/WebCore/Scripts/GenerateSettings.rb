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

parsedSettings = begin
  YAML.load_file(options[:input])
rescue ArgumentError => e
  puts "Could not parse input file: #{e.message}"
  exit(-1)
end

class Setting
  attr_accessor :name
  attr_accessor :type
  attr_accessor :initial
  attr_accessor :excludeFromInternalSettings
  attr_accessor :conditional
  attr_accessor :onChange
  attr_accessor :getter
  attr_accessor :inspectorOverride
  
  def initialize(name, opts)
    @name = name
    @type = opts["type"] || "bool"
    @initial = opts["initial"]
    @excludeFromInternalSettings = opts["excludeFromInternalSettings"] || false
    @conditional = opts["conditional"]
    @onChange = opts["onChange"]
    @getter = opts["getter"]
    @inspectorOverride = opts["inspectorOverride"]
  end

  def valueType?
    @type != "String" && @type != "URL"
  end

  def idlType
    # FIXME: Add support for more types including enumerate types.
    if @type == "int"
      "long"
    elsif @type == "unsigned"
      "unsigned long"
    elsif @type == "double"
      "double"
    elsif @type == "float"
      "float"
    elsif @type == "String"
      "DOMString"
    elsif @type == "bool"
      "boolean"
    else
      nil
    end
  end

  def parameterType
    if valueType?
      @type
    else
      "const #{@type}&"
    end
  end

  def hasComplexSetter?
    @onChange != nil
  end

  def hasComplexGetter?
    hasInspectorOverride?
  end

  def setterFunctionName
    if @name.start_with?("css", "xss", "ftp", "dom", "dns", "ice", "hdr")
      "set" + @name[0..2].upcase + @name[3..@name.length]
    else
      "set" + @name[0].upcase + @name[1..@name.length]
    end
  end

  def getterFunctionName
    @getter || @name
  end

  def hasInspectorOverride?
    @inspectorOverride == true
  end
end

class Conditional
  attr_accessor :condition
  attr_accessor :settings
  attr_accessor :boolSettings
  attr_accessor :nonBoolSettings
  attr_accessor :settingsWithComplexGetters
  attr_accessor :settingsWithComplexSetters

  def initialize(condition, settings)
    @condition = condition
    @settings = settings
    
    @boolSettings = @settings.select { |setting| setting.type == "bool" }
    @nonBoolSettings = @settings.reject { |setting| setting.type == "bool" }
    @settingsWithComplexGetters = @settings.select { |setting| setting.hasComplexGetter? }
    @settingsWithComplexSetters = @settings.select { |setting| setting.hasComplexSetter? }
  end
end

class Settings
  attr_accessor :settings
  attr_accessor :unconditionalSetting
  attr_accessor :unconditionalBoolSetting
  attr_accessor :unconditionalNonBoolSetting
  attr_accessor :unconditionalSettingWithComplexGetters
  attr_accessor :unconditionalSettingWithComplexSetters
  attr_accessor :conditionals
  
  def initialize(hash)
    @settings = []
    hash.each do |name, options|
      @settings << Setting.new(name, options)
    end
    @settings.sort! { |x, y| x.name <=> y.name }
    
    @unconditionalSetting = @settings.reject { |setting| setting.conditional }
    @unconditionalBoolSetting = @unconditionalSetting.select { |setting| setting.type == "bool" }
    @unconditionalNonBoolSetting = @unconditionalSetting.reject { |setting| setting.type == "bool" }
    @unconditionalSettingWithComplexGetters = @unconditionalSetting.select { |setting| setting.hasComplexGetter? }
    @unconditionalSettingWithComplexSetters = @unconditionalSetting.select { |setting| setting.hasComplexSetter? }
    @inspectorOverrideSettings = @settings.select { |setting| setting.hasInspectorOverride? }

    @conditionals = []
    conditionalsMap = {}
    @settings.select { |setting| setting.conditional }.each do |setting|
      if !conditionalsMap[setting.conditional]
        conditionalsMap[setting.conditional] = []
      end

      conditionalsMap[setting.conditional] << setting
    end
    conditionalsMap.each do |key, value|
      @conditionals << Conditional.new(key, value)
    end
    @conditionals.sort! { |x, y| x.condition <=> y.condition }
  end

  def renderToFile(template, file)
    template = File.join(File.dirname(__FILE__), template)

    output = ERB.new(File.read(template), 0, "-").result(binding)
    File.open(file, "w+") do |f|
      f.write(output)
    end
  end
end

settings = Settings.new(parsedSettings)
settings.renderToFile("SettingsTemplates/Settings.h.erb", File.join(options[:outputDirectory], "Settings.h"))
settings.renderToFile("SettingsTemplates/Settings.cpp.erb", File.join(options[:outputDirectory], "Settings.cpp"))
settings.renderToFile("SettingsTemplates/InternalSettingsGenerated.idl.erb", File.join(options[:outputDirectory], "InternalSettingsGenerated.idl"))
settings.renderToFile("SettingsTemplates/InternalSettingsGenerated.h.erb", File.join(options[:outputDirectory], "InternalSettingsGenerated.h"))
settings.renderToFile("SettingsTemplates/InternalSettingsGenerated.cpp.erb", File.join(options[:outputDirectory], "InternalSettingsGenerated.cpp"))

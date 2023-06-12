#!/usr/bin/env ruby
#
# Copyright (c) 2017-2023 Apple Inc. All rights reserved.
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
require 'psych'

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

options[:preferenceFiles] = ARGV.shift(ARGV.size)
if options[:preferenceFiles].empty?
  puts optparse
  exit 1
end

FileUtils.mkdir_p(options[:outputDirectory])

class Preference
  attr_accessor :name
  attr_accessor :opts
  attr_accessor :type
  attr_accessor :refinedType
  attr_accessor :status
  attr_accessor :category
  attr_accessor :defaultsOverridable
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
    @category = opts["category"] || "none"
    @defaultsOverridable = opts["defaultsOverridable"] || false
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

  def webFeatureCategory
    if %w{ css dom html }.include?(@category)
      "WebFeatureCategory" + @category.upcase
    else
      "WebFeatureCategory" + @category.capitalize
    end
  end

  def apiStatus
    "API::FeatureStatus::" + @status.capitalize
  end

  def apiCategory
      if %w{ css dom html }.include?(@category)
        "API::FeatureCategory::" + @category.upcase
      else
        "API::FeatureCategory::" + @category.capitalize
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
    %w{ unstable internal testable }.include? @status
  end

  def defaultsOverridable?
    @defaultsOverridable
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
    %w{ testable preview stable mature }.include? @status
  end


  # Corresponds to WebFeatureStatus enum cases. "developer" and up require human-readable names.
  STATUSES = %w{ embedder unstable internal developer testable preview stable mature }

  # Corresponds to WebFeatureCategory enum cases.
  CATEGORIES = %w{ animation css dom html javascript media networking privacy security }

  def self.parse_for_frontend(filename, frontend)
    document = Psych.parse_file(filename)
    previousName = nil
    failed = false
    error = proc { |msg, node| STDERR.puts("#{filename}:#{node.start_line+1}:#{node.start_column+1}: error: #{msg}") ; failed = true }
    result = []

    document.root.children.each_slice(2) do |key, mapping|
      # Parse the preference name and check sort order.
      name = key.value
      if previousName and previousName > name
        error.call("#{name}: preference keys not sorted, this preference should come before #{previousName}", key)
      end
      previousName = name

      # Form a hash of preference options, but keep the nodes handy for error
      # reporting.
      nodes = {}
      mapping.children.each_slice(2) do |lhs, rhs|
        nodes[lhs.value] = rhs
      end

      # Validate the status and default value. WebCore settings do not have a
      # status.
      status = nodes["status"]&.value
      if frontend != "WebCore"
        error.call("#{name}: missing required status field", mapping) and next unless status
      end
      defaultValue = nodes["defaultValue"].to_ruby
      webcoreSettingOnly = !nodes["webcoreBinding"] && defaultValue.keys == ["WebCore"]

      if status && !STATUSES.include?(status)
        error.call("#{name}: not one of the known statuses: #{STATUSES}", nodes["status"])
        next
      end

      if status && %w{ unstable internal developer testable preview stable }.include?(status)
        # Reject a user-visible preference without a humanReadableName.
        error.call("#{name}: status \"#{status}\" indicates this preference may be visible in UI, but it has no humanReadableName", mapping) and next if !nodes["humanReadableName"]

        # Reject a user-visible preference with a default value in SOME
        # frontends but not others.
        error.call("#{name}: status \"#{status}\" indicates this preference may be visible in UI, and has a default value bound to WebCore::Settings, so it must have default values for all frontends", nodes["defaultValue"]) and next if webcoreSettingOnly

        if %w{ developer testable preview stable }.include?(status)
            error.call("#{name}: missing required category field", mapping) and next if !nodes["category"]
            category = nodes["category"]
            error.call("#{name}: not one of the known categories: #{CATEGORIES}", category) and next unless CATEGORIES.include?(category.value)
        end
      elsif webcoreSettingOnly and frontend != "WebCore"
        next
      end

      if defaultValue.include?(frontend)
        preference = self.new(name, nodes.transform_values(&:to_ruby), frontend)
        result << preference
      end
    end
    exit 1 if failed
    result
  end
end

class Preferences
  attr_accessor :preferences

  def initialize(preferenceFiles, frontend)
    @frontend = frontend

    @preferences = []
    preferenceFiles.each do |file|
      @preferences += Preference.parse_for_frontend(file, frontend)
    end

    @preferences.sort_by! { |p| p.humanReadableName.empty? ? p.name : p.humanReadableName }
    @exposedPreferences = @preferences.select { |p| p.exposed }
    @exposedFeatures = @exposedPreferences.select { |p| p.type == "bool" }

    @preferencesBoundToSetting = @preferences.select { |p| !p.webcoreBinding }
    @preferencesBoundToDeprecatedGlobalSettings = @preferences.select { |p| p.webcoreBinding == "DeprecatedGlobalSettings" }

    @warning = "THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT."
  end

  def parse(filename)
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

#!/usr/bin/env ruby
#
# Copyright (C) 2026 Apple Inc. All rights reserved.
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

# Generates QuirkBehaviorID, the QuirkBehaviors:: constants, and the QuirksAccessors base class of Quirks
# from QuirkBehaviors.yaml.

require "fileutils"
require 'erb'
require 'optparse'
require 'yaml'

options = {
  :outputDirectory => Dir.getwd,
  :templates => [],
}
optparse = OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename($0)} [--outputDir <output>] --template <file> [--template <file>...] <QuirkBehaviors.yaml>"

  opts.separator ""

  opts.on("--template input", "template to use for generation (may be specified multiple times)") { |template| options[:templates] << template }
  opts.on("--outputDir output", "directory to generate file in (default: cwd)") { |outputDir| options[:outputDirectory] = outputDir }
  opts.on("-h", "--help", "show this help message") { puts opts; exit 1 }
end

optparse.parse!

if ARGV.size != 1
  puts optparse
  exit 1
end
behaviorsFile = ARGV.shift

FileUtils.mkdir_p(options[:outputDirectory])

def loadBehaviors(path)
  document = begin
    YAML.parse_file(path)
  rescue Psych::SyntaxError => e
    STDERR.puts "error: Could not parse input file #{path}: #{e.message}"
    exit(1)
  end
  root = document && document.children[0]
  if !root.is_a?(Psych::Nodes::Mapping)
    STDERR.puts "error: Input file #{path} is not a mapping of QuirkBehaviorID names to their fields."
    exit(1)
  end
  root.children.each_slice(2).map { |key, value| [key.value, value.to_ruby] }
end

class QuirkBehavior
  attr_reader :id, :parameters, :conditions, :accessor

  def initialize(id, opts)
    @id = id
    @available = opts["available"] || "always"
    @parameters = opts["parameters"] || []
    @isStatic = opts["static"] || false
    @conditions = opts["conditions"] || []
    @requiredConditions = opts["conditionsRequired"] ? @conditions : []
    @accessor = opts["implementation"] == "custom" ? nil : (opts["accessor"] || constantName)
    @isExported = opts["export"] || false
  end

  def constantName
    @id[0].downcase + @id[1..-1]
  end

  def availableExpression
    @available.gsub(/\b([A-Za-z_]\w*)\b/, 'BuildCondition::\1')
  end

  def initializer
    fields = [".id = QuirkBehaviorID::#{@id}", ".isAvailable = #{availableExpression}"]
    fields << ".quirkParametersNeeded = #{optionSet("QuirkParametersNeeded", @parameters.map { |parameter| "Needs#{parameter}" })}" if !@parameters.empty?
    fields << ".quirkConditionsSupported = #{optionSet("QuirkConditionsSupported", @conditions)}" if !@conditions.empty?
    fields << ".quirkConditionsNeeded = #{optionSet("QuirkConditionsSupported", @requiredConditions)}" if !@requiredConditions.empty?
    "{ #{fields.join(", ")} }"
  end

  def hasGeneratedAccessor?
    !@accessor.nil?
  end

  def shape
    return :staticURL if @isStatic
    return :secondaryURL if @conditions == ["SecondaryURL"]
    return :node if @conditions == ["ElementSelector"]
    nil
  end

  def argument
    case shape
    when :staticURL, :secondaryURL then "const URL&"
    when :node then "const Node&"
    end
  end

  # Only the argument-less body is free of refcounting; the others copy a URL or hold a RefPtr.
  def isNoDelete?
    shape.nil?
  end

  def parameterList
    argument ? "#{argument} #{shape == :node ? "node" : "url"}" : ""
  end

  def declaration
    prefix = @isExported ? "WEBCORE_EXPORT " : ""
    prefix += "static " if @isStatic
    nodelete = isNoDelete? ? "NODELETE " : ""
    "#{prefix}bool #{nodelete}#{@accessor}(#{argument})#{constSuffix};"
  end

  def definitionSignature
    "bool QuirksAccessors::#{@accessor}(#{parameterList})#{constSuffix}"
  end

  def body
    id = "QuirkBehaviorID::#{@id}"
    case shape
    when :staticURL
      "return resolveTopURLQuirks(url).isBehaviorEnabled(#{id});"
    when :secondaryURL
      "return needsQuirks() && m_quirksData.behaviorAppliesToURL(#{id}, url);"
    when :node
      "return needsQuirks() && behaviorAppliesToNode(#{id}, &node);"
    else
      "return needsQuirks() && m_quirksData.isBehaviorEnabled(#{id});"
    end
  end

  private

  def constSuffix
    @isStatic ? "" : " const"
  end

  def optionSet(type, values)
    values = values.map { |value| "#{type}::#{value}" }
    values.size == 1 ? values[0] : "OptionSet<#{type}> { #{values.join(", ")} }"
  end
end

class QuirkBehaviors
  FIELDS = %w{ available parameters conditions conditionsRequired implementation accessor static export }
  GENERATED_ONLY_FIELDS = %w{ accessor static export }
  PARAMETERS = %w{ Script UserAgent ChromeCompatibilityVersion CookieNames }
  CONDITIONS = %w{ ElementSelector SecondaryURL DocumentSelector }

  attr_reader :behaviors

  def initialize(path)
    @behaviors = []
    @warning = "THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT."

    failed = false
    reject = Proc.new do |msg|
      STDERR.puts("error: #{path}: " + msg)
      failed = true
    end

    seen = {}
    accessorNames = {}
    loadBehaviors(path).each do |id, opts|
      if seen[id]
        reject.call "#{id} is defined more than once."
        next
      end
      seen[id] = true
      opts ||= {}
      reject.call "#{id} is not a valid QuirkBehaviorID name." if !(id =~ /\A[A-Z]\w*\z/)
      (opts.keys - FIELDS).each { |field| reject.call "#{id} has unknown field \"#{field}\". Allowed fields: #{FIELDS.join(", ")}." }
      ((opts["parameters"] || []) - PARAMETERS).each { |parameter| reject.call "#{id} has unknown parameter \"#{parameter}\"." }
      ((opts["conditions"] || []) - CONDITIONS).each { |condition| reject.call "#{id} has unknown condition \"#{condition}\"." }

      behavior = QuirkBehavior.new(id, opts)
      generated = behavior.hasGeneratedAccessor?
      reject.call "#{id} has implementation \"#{opts["implementation"]}\"; the only value is \"custom\"." if opts["implementation"] && opts["implementation"] != "custom"
      (GENERATED_ONLY_FIELDS & opts.keys).each { |field| reject.call "#{id} has a custom implementation, so \"#{field}\" would be ignored." } if !generated
      reject.call "#{id} has parameters, which a generated accessor cannot read. Use \"implementation: custom\"." if generated && !behavior.parameters.empty?
      reject.call "#{id} is static, so it only sees top-URL quirks and cannot evaluate conditions." if generated && behavior.shape == :staticURL && !behavior.conditions.empty?
      reject.call "#{id} has conditions #{behavior.conditions}; a generated accessor supports only [ElementSelector] or [SecondaryURL]. Use \"implementation: custom\"." if generated && !behavior.conditions.empty? && !behavior.shape
      reject.call "#{id}'s accessor #{behavior.accessor} is also generated for #{accessorNames[behavior.accessor]}." if generated && accessorNames[behavior.accessor]
      accessorNames[behavior.accessor] = id if generated
      reject.call "#{id} has conditionsRequired but no conditions." if opts["conditionsRequired"] && behavior.conditions.empty?
      reject.call "#{id} has accessor #{behavior.accessor}, which is already the default; remove it." if opts["accessor"] && opts["accessor"] == behavior.constantName

      @behaviors << behavior
    end
    exit 1 if failed
  end

  def createTemplate(templateString)
    ERB.new(templateString, trim_mode:"-")
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

quirkBehaviors = QuirkBehaviors.new(behaviorsFile)

options[:templates].each do |template|
  quirkBehaviors.renderTemplate(template, options[:outputDirectory])
end

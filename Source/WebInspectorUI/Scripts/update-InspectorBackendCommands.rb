#!/usr/bin/ruby

require 'fileutils'
require 'tempfile'
require 'tmpdir'

if ARGV.size != 0
  puts "usage: #{File.basename $0}"
  exit 1
end

WEB_INSPECTOR_PATH = File.expand_path File.join(File.dirname(__FILE__), "..")
JAVASCRIPTCORE_PATH = File.expand_path File.join(File.dirname(__FILE__), "..", "..", "JavaScriptCore")
WEBCORE_PATH = File.expand_path File.join(File.dirname(__FILE__), "..", "..", "WebCore")

JAVASCRIPT_INSPECTOR_PROTOCOLS_PATH = File.join JAVASCRIPTCORE_PATH, "inspector", "protocol"
WEB_INSPECTOR_PROTOCOLS_PATH = File.join WEBCORE_PATH, "inspector", "protocol"

$code_generator_path = File.join JAVASCRIPTCORE_PATH, "inspector", "scripts", "CodeGeneratorInspector.py"
$inspector_json_generator_path = File.join JAVASCRIPTCORE_PATH, "inspector", "scripts", "generate-combined-inspector-json.py"
$inspector_js_json_path = Tempfile.new("generated-InspectorJS.json").path
$inspector_web_json_path = Tempfile.new("generated-InspectorWeb.json").path
$versions_directory_path = File.join WEB_INSPECTOR_PATH, "Versions"
$web_inspector_user_interface_path = File.join WEB_INSPECTOR_PATH, "UserInterface"
$web_inspector_protocol_path = File.join WEB_INSPECTOR_PATH, "UserInterface", "Protocol"

class Task
  def initialize(input_json_path, dependency_json_path, type, output_directory_path, verification)
    @type = type
    @input_json_path = input_json_path
    @dependency_json_path = dependency_json_path
    @output_directory_path = output_directory_path
    @verification = verification
  end

  def run
    output_filename_prefix = {"JavaScript" => "JS", "Web" => "Web"}[@type]
    output_filename = "Inspector#{output_filename_prefix}BackendCommands.js"
    display_input = File.basename @input_json_path
    display_output = File.join @output_directory_path.gsub(/^.*?\/UserInterface/, "UserInterface"), output_filename
    puts "#{display_input} -> #{display_output}"

    Dir.mktmpdir do |tmpdir|
      dependency = @dependency_json_path ? "'#{@dependency_json_path}'" : ""
      cmd = "#{$code_generator_path} '#{@input_json_path}' #{dependency} --output_h_dir '#{tmpdir}' --output_cpp_dir '#{tmpdir}' --output_js_dir '#{tmpdir}' --write_always --output_type '#{@type}'"
      cmd += " --no_verification" if !@verification
      %x{ #{cmd} }
      if $?.exitstatus != 0
        puts "ERROR: Error Code (#{$?.exitstatus}) Evaluating: #{cmd}"
        exit 1
      end

      generated_path = File.join tmpdir, output_filename
      if !File.exists?(generated_path)
        puts "ERROR: Generated file does not exist at expected path."
        exit 1
      end

      FileUtils.mkdir_p @output_directory_path
      FileUtils.cp generated_path, @output_directory_path
    end
  end
end

def generate_combined_inspector_json
  # InspectorJS.json
  output = %x{ #{$inspector_json_generator_path} "#{JAVASCRIPT_INSPECTOR_PROTOCOLS_PATH}" > "#{$inspector_js_json_path}" }
  if $?.exitstatus != 0
    puts "ERROR: Could not generate combined InspectorJS.json file"
    exit 1
  end

  # InspectorWeb.json
  output = %x{ #{$inspector_json_generator_path} "#{WEB_INSPECTOR_PROTOCOLS_PATH}" > "#{$inspector_web_json_path}" }
  if $?.exitstatus != 0
    puts "ERROR: Could not generate combined InspectorWeb.json file"
    exit 1
  end
end

def all_tasks
  tasks = []

  # ToT Inspector json files.
  generate_combined_inspector_json
  tasks << Task.new($inspector_js_json_path, nil, "JavaScript", $web_inspector_protocol_path, true)
  tasks << Task.new($inspector_web_json_path, $inspector_js_json_path, "Web", $web_inspector_protocol_path, true)

  had_error = false
  Dir.glob(File.join($versions_directory_path, "*.json")).each do |version_path|
    match = File.basename(version_path).match(/^Inspector(.*?)\-([^-]+?)\.json$/)
    if match
      output_path = File.join $web_inspector_user_interface_path, "Legacy", match[2]
      tasks << Task.new(version_path, nil, "Web", output_path, false)
    else
      puts "ERROR: Version file (#{version_path}) did not match the template Inspector<ANYTHING>-<VERSION>.js"
      had_error = true
    end
  end
  exit 1 if had_error

  tasks
end

def main
  all_tasks.each { |task| task.run }
end

main

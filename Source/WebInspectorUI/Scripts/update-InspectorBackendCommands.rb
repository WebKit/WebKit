#!/usr/bin/ruby

require 'fileutils'
require 'tmpdir'

if ARGV.size != 0
  puts "usage: #{File.basename $0}"
  exit 1
end

WEB_INSPECTOR_PATH = File.expand_path File.join(File.dirname(__FILE__), "..")
WEBCORE_PATH = File.expand_path File.join(File.dirname(__FILE__), "..", "..", "WebCore")

$code_generator_path = File.join WEBCORE_PATH, "inspector", "CodeGeneratorInspector.py"
$inspector_json_path = File.join WEBCORE_PATH, "inspector", "Inspector.json"
$versions_directory_path = File.join WEB_INSPECTOR_PATH, "Versions"
$web_inspector_user_interface_path = File.join WEB_INSPECTOR_PATH, "UserInterface"
$backend_commands_filename = "InspectorBackendCommands.js"


class Task
  def initialize(input_json_path, output_directory_path, verification)
    @input_json_path = input_json_path
    @output_directory_path = output_directory_path
    @verification = verification
  end

  def run
    display_input = File.basename @input_json_path
    display_output = File.join @output_directory_path.gsub(/^.*?\/UserInterface/, "UserInterface"), $backend_commands_filename
    puts "#{display_input} -> #{display_output}"

    Dir.mktmpdir do |tmpdir|
      cmd = "#{$code_generator_path} '#{@input_json_path}' --output_h_dir '#{tmpdir}' --output_cpp_dir '#{tmpdir}' --output_js_dir '#{tmpdir}' --write_always"
      cmd += " --no_verification" if !@verification
      %x{ #{cmd} }
      if $?.exitstatus != 0
        puts "ERROR: Error Code (#{$?.exitstatus}) Evaluating: #{cmd}"
        exit 1
      end

      generated_path = File.join tmpdir, $backend_commands_filename
      if !File.exists?(generated_path)
        puts "ERROR: Generated file does not exist at expected path."
        exit 1
      end

      FileUtils.mkdir_p @output_directory_path
      FileUtils.cp generated_path, @output_directory_path
    end
  end
end

def all_tasks
  tasks = []
  tasks << Task.new($inspector_json_path, $web_inspector_user_interface_path, true)

  had_error = false
  Dir.glob(File.join($versions_directory_path, "*.json")).each do |version_path|
    match = File.basename(version_path).match(/^Inspector(.*?)\-([^-]+?)\.json$/)
    if match
      output_path = File.join $web_inspector_user_interface_path, "Legacy", match[2]
      tasks << Task.new(version_path, output_path, false)
    else
      puts "ERROR: Version file (#{version_path}) did not the template Inspector<ANYTHING>-<VERSION>.js"
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

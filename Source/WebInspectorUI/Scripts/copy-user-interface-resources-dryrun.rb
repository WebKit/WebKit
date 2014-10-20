#!/usr/bin/ruby

require 'fileutils'
require 'tmpdir'

if ARGV.size != 1
  puts "usage: #{File.basename $0} <output-directory>"
  exit 1
end

WEB_INSPECTOR_PATH = File.expand_path File.join(File.dirname(__FILE__), "..")
COPY_USER_INTERFACE_RESOURCES_PATH = File.join WEB_INSPECTOR_PATH, "Scripts", "copy-user-interface-resources.pl"

$output_directory = File.expand_path ARGV[0]

Dir.mktmpdir do |tmpdir|

  # Create the output directory if needed.
  FileUtils.mkdir_p $output_directory
  
  # Create empty derived sources expected to exist.
  FileUtils.touch(File.join(tmpdir, 'InspectorBackendCommands.js'))
  
  # Setup the environment and run.
  ENV["DERIVED_SOURCES_DIR"] = tmpdir
  ENV["JAVASCRIPTCORE_PRIVATE_HEADERS_DIR"] = tmpdir
  ENV["SRCROOT"] = WEB_INSPECTOR_PATH
  ENV["TARGET_BUILD_DIR"] = $output_directory
  ENV["UNLOCALIZED_RESOURCES_FOLDER_PATH"] = ""
  ENV["COMBINE_INSPECTOR_RESOURCES"] = "YES"
  exec COPY_USER_INTERFACE_RESOURCES_PATH

end

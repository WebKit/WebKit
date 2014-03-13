#!/usr/bin/ruby

require "find"

$verbose = ARGV.include?("--verbose");
$remove_console_asserts_path = File.expand_path File.join(File.dirname(__FILE__), "remove-console-asserts.pl")
$web_inspector_user_interface_path = File.expand_path File.join(File.dirname(__FILE__), "..", "UserInterface")

Find.find($web_inspector_user_interface_path) do |path|
  # Skip directories, External, Images, and non-js.
  next if File.directory?(path)
  next if path =~ /\/(External|Images)\//
  next if path !~ /\.js$/

  # Run remove-console-asserts on each file.
  puts "Checking: #{path} ..." if $verbose
  output = %x{ perl '#{$remove_console_asserts_path}' --input-script '#{path}' --output-script /dev/null }
  if !output.empty?
    puts "#{File.basename(path)}:"
    puts output
    puts
  end
end

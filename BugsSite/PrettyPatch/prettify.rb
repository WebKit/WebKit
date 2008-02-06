#!/usr/bin/ruby

require 'PrettyPatch'

patch_data = nil
if ARGV.length == 0 || ARGV[0] == '-' then
    patch_data = $stdin.read
else
    File.open(ARGV[0]) { |file| patch_data = file.read }
end

puts PrettyPatch.prettify(patch_data)

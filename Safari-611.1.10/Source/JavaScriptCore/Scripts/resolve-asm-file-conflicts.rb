#!/usr/bin/env ruby

# Copyright (C) 2020 Igalia S. L.
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

# Resolve conflicts in `.file` directives in assembler files. Those
# may result from inline asm statements which themselves have `.file`
# directives. As the inline asm has no idea what the next free file
# number is, it'll generally have its own numbering. To get things to
# work we have to
# 1. remap conflicting file numbers
# 2. change `.loc` directives to reference the appropriate file number.
#
# To be able to do that, we need some concept of "scope", i.e. which
# set of files a given `.loc` directive refers to. We get that by
# tracking the #APP/#NOAPP directives that the compiler emits when it
# switch to/from inline asm.
#
# In effect, we convert
#     .file 1 "foo"
#     #APP
#     .file 1 "bar"
#     .file 2 "foo"
#     .loc 1, X
#     #NOAPP
#     .loc 1, Y
# to
#     .file 1 "foo"
#     #APP
#     .file 2 "bar"
#     .file 1 "foo"
#     .loc 2, X
#     #NOAPP
#     .loc 1, Y

require 'pathname'
require 'stringio'
require 'strscan'

ParseResultSuccess = Struct.new(:str)
ParseResultError = Struct.new(:error)

# Parses the single string literal following a .file assembler directive
class FileDirectiveArgScanner
  def initialize(s)
    @s = StringScanner.new(s)
  end
  def parse
    @s.skip(/\s*/)

    # We require at least one string literal
    ret = parse_string_literal
    if ret.respond_to?(:error)
      return ret
    end

    @s.skip(/\s*/)
    if not @s.eos?
      return ParseResultError.new("Expected end of line after #{ret.str}")
    end
    return ParseResultSuccess.new(ret.str)
  end
  def parse_string_literal
    if @s.scan(/"/).nil?
      err = "Expected string literal at `#{@s.string}` (pos #{@s.pos})"
      return ParseResultError.new(err)
    end
    parse_until_end_of_string_literal
  end
  def parse_until_end_of_string_literal
    start_pos = @s.pos
    while true
      # Search for our special characters
      @s.skip(/[^"\\]+/)
      if @s.scan(/\\/)
        if @s.scan(/\\/)
          # When we see '\\', consume both characters so that the
          # second '\' will not be treated as an escape char.
          next
        elsif @s.scan(/"/)
          # For '\"', consume both characters so that the '"' will not
          # terminate the string.
          next
        end
        next
      elsif @s.scan(/"/)
        # '"' without a preceeding '\'; terminate the literal.
        # We're already past the '"', so the literal ends at -2
        # characters.
        return ParseResultSuccess.new(@s.string[start_pos..(@s.pos - 2)])
      elsif @s.eos?
        err = "Unterminated string literal (starting at pos #{start_pos} in #{@s.string}"
        return ParseResultError.new(err)
      end
      raise "Internal error (#{@s.inspect})"
    end
  end
end

def test(outf, str, res)
  pr = FileDirectiveArgScanner.new(str).parse
  if res.is_a?(Array)
    if pr.respond_to?(:error)
      outf.puts("Parse result is `#{pr.error}` but expected #{res}")
      return false
    end
    if pr.str != res[0]
      outf.puts("Parsed path `#{pr.str}` but expected `#{res[0]}`")
      return false
    end
    return true
  elsif res.is_a?(String)
    if pr.respond_to?(:error)
      if not pr.error.downcase.include?(res.downcase)
        err = "Error message `#{pr.error}` does not include expected substring `#{res}`"
        outf.puts(err)
        return false
      end
      return true
    end
    outf.puts("Expected error (#{res}) but got successful parse #{pr.str}")
    return false
  else
    raise "Internal error #{res.class}"
  end
end

def selftest
  nr_succeeded = 0
  tests = [
    # simple string
    ['"foo/bar"', ["foo/bar"]],

    ['"foo', "Unterminated string literal"],

    ['"foo\"', "Unterminated string literal"],

    # "foo\"
    ["\"foo\\\"", "Unterminated string literal"],

    # "foo\\"
    ["\"foo\\\\\"", ["foo\x5c\x5c"]],

    # Can escape '"'
    ['"foo\"bar"', ['foo\"bar']],

    # Can parse relative
    ['"foo/bar"', ['foo/bar']],

    # Can parse absolute
    ['"/bar"', ['/bar']],

    # Can parse absolute with two components
    ['"/bar/baz"', ['/bar/baz']],

    # Can detect stray token
    ['"foo" bar', "Expected end of line"],

    # Can detect stray token without whitespace
    ['"foo"bar', "Expected end of line"],

    # Will not accept clang-style .file directives
    ['"working_directory" "path"', "Expected end of line"]
  ]
  outf = StringIO.new("")
  tests.each { |str, res|
    if test(outf, str, res)
      nr_succeeded += 1
    end
  }
  if nr_succeeded != tests.size
    $stderr.puts(outf.string)
    $stderr.puts("Some self tests failed #{nr_succeeded}/#{tests.size}")
    exit(3)
  end
end

# Keep track of which fileno is assigned to which file path. We call
# the fileno a 'slot'.
class FileSlotTracker
  @@next_free = 1   # Next free global slot
  @@paths = {} # Maps path -> global slot
  def initialize(parent)
    @parent = parent
    # This maps from our own local slots (i.e. within an APP section)
    # to a global slot.
    @slot_map = {}
  end
  # We make sure that .file directives that specify the same path are
  # dedup'd, i.e. if we see
  #     .file N "/path/to/include.h"
  #     ...
  #     .file M "/path/to/include.h"
  # then the "local" N, M slots will be mapped to the same "global" slot.
  def register_path(path, slot)
    curr_slot = @@paths[path]
    if curr_slot.nil?
      # We haven't seen this file before
      if slot <= @@next_free
        # Desired file slot either clashes with an existing one, or is
        # the next one to be allocated. In either case, assign the
        # next free slot.
        assign_next_slot(path)
      else
        # Don't allow slot gaps. Technically we could handle them, but
        # they should never happen; bail now rather than papering over
        # an earlier error.
        $stderr.puts("File wants slot #{slot} but only seen #{@@next_free} so far")
        exit(2)
      end
    else
      # We've already assigned a slot for this file.
    end
    @slot_map[slot] = @@paths[path]
    if @slot_map[slot].nil?
      raise "Didn't map local slot #{slot}"
    end
  end
  # Return global slot for path
  def slot_for_path(path)
    return @@paths[path]
  end
  # Return global slot that will replace the local slot
  def remap_slot(slot)
    ret = nil
    if @slot_map.size > 0
      # If the current NO_APP segment has defined a .file, only look
      # in the current FileSlotTracker. This is the case for a
      # top-level inline asm statement.
      ret = @slot_map[slot]
    elsif not @parent.nil?
      # If the current NO_APP segment has not defined a .file, clearly
      # all .loc directives refer to files defined in the APP
      # part. This is the case for non-top-level inline asm
      # statements.
      ret = @parent.remap_slot(slot)
    end
    if ret.nil?
      raise "No global slot for #{slot}"
    end
    ret
  end
  private
  def assign_next_slot(path)
    @@paths[path] = @@next_free
    @@next_free += 1
  end
end

# Return sequential lines, while keeping track of whether we're in an
# #APP or #NOAPP section.
class AsmReader
  attr_reader :in_app
  attr_accessor :app_to_noapp, :noapp_to_app
  def initialize(f)
    @f = f
    @f.rewind
    @linenr = 0
    @in_app = false
    @last_switch = "start of file" # For error messages
  end
  def next_line
    while true
      l = @f.gets
      if l.nil?
        return l
      end
      @linenr += 1
      if /^#\s*APP\s*/.match(l)
        if @in_app
          raise "#APP on line #{@linenr} but already in #APP (#{@last_switch})"
        end
        @in_app = true
        @last_switch = @linenr
        if @noapp_to_app
          @noapp_to_app.call()
        end
      end
      if /^#\s*NO_APP\s*/.match(l)
        if not @in_app
          raise "#NOAPP on line #{@linenr} but was not in #APP (last swich at #{@last_switch})"
        end
        @in_app = false
        @last_switch = @linenr
        if @app_to_noapp
          @app_to_noapp.call()
        end
      end
      return l
    end
  end
end

class FileConflictResolver
  @@file_re = /^\s*[.]file\s+(?<slot>\d+)\s+(?<rest>.*)$/
  @@loc_re = /^(?<white1>\s*)[.]loc(?<white2>\s+)(?<slot>\d+)(?<rest>\s+\d+.*)$/
  def initialize(inf, outf)
    @outf = outf
    @trackers = [FileSlotTracker.new(nil)]
    @asm_reader = AsmReader.new(inf)
    # When we enter an #APP section (i.e. asm emitted from an expanded
    # inline asm statement), create a new file tracker, in effect
    # processing the .file and .loc directives in a new "namespace"
    # (as far as the file numbers are concerned). This is an array,
    # but in practice the size will either be 1 (NOAPP) or 2 (APP).
    @asm_reader.app_to_noapp = Proc.new { ||
                                          @trackers.pop
    }
    @asm_reader.noapp_to_app = Proc.new { ||
                                          @trackers.push(FileSlotTracker.new(@trackers[-1]))
    }
  end
  def run
    while true
      l = @asm_reader.next_line
      break unless l

      md = @@file_re.match(l)
      if md
        file_directive(md)
        next
      end
      md = @@loc_re.match(l)
      if md
        loc_directive(md)
        next
      end
      @outf.write(l)
      next
    end
  end
  def loc_directive(md)
    tracker = @trackers.last
    slot = tracker.remap_slot(md[:slot].to_i)
    @outf.puts("#{md[:white1]}.loc#{md[:white2]}#{slot}#{md[:rest]}")
  end
  def file_directive(md)
    tracker = @trackers.last

    pr = FileDirectiveArgScanner.new(md[:rest]).parse
    if pr.respond_to?(:error)
      $stderr.puts("Error parsing path argument to .file directive: #{pr.error}")
      exit(2)
    end

    path = pr.str
    tracker.register_path(path, md[:slot].to_i)

    slot = tracker.slot_for_path(path)
    @outf.puts("\t.file\t#{slot} #{md[:rest]}")
  end
end

# First, make sure our tests still pass. This only takes a fraction of
# our runtime and ensures the tests will get run by anyone trying out
# changes to this file.
selftest

if ARGV.size != 2
  $stderr.puts("Usage: #{$0} input output")
  exit(2)
end

inpath, outpath = ARGV.collect { |n| Pathname.new(n) }

if not inpath.file?
  $stderr.puts("Not a regular file: `#{inpath}`")
  exit(2)
end

if inpath.extname.upcase != ".S"
  $stderr.puts("warning: file `#{inpath}` doesn't have a `.s` or `.S` extension. Going on anyway...")
end

File.open(inpath, "r") { |inf|
  File.open(outpath, "w") { |outf|
    FileConflictResolver.new(inf, outf).run
  }
}

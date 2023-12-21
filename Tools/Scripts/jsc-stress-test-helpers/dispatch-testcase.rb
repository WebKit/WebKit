#!/usr/bin/env ruby
# Copyright (C) 2023 Igalia S.L. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

require 'open3'
require 'pathname'
require 'shellwords'

STATUS_FILE_PREFIX = "test_status_"
STATUS_FILE_PASS = "P"
STATUS_FILE_FAIL = "F"

def loadVars(path)
    ret = {}
    File.open(path, "r") { |f|
        linenr = 0
        f.each_line { |l|
            linenr += 1
            md = /(\w+)=(.*)/.match(l)
            if md.nil?
                $stderr.puts("Could not parse line #{path}:#{linenr} `#{l}`")
                exit(3)
            end
            key = md[1]
            words = Shellwords.split(md[2])
            value = nil
            if words.size == 0
                value = ""
            elsif words.size == 1
                value = words[0]
            else
                raise "Malformed line `#{l}` in #{path}"
            end
            ret[key] = value
        }
    }
    ret
end

def envVars(vars)
    ret = {}
    vars["EXPORT_VARS"].split.each { |name|
        ret[name] = vars[name]
    }
    ret
end

def hasDiff
    case RbConfig::CONFIG["host_os"]
    when /mswin|mingw|cygwin/
        _out, _err, status = Open3.capture3("where", "/q", "diff")
        return status.success?
    else
        _out, _err, status = Open3.capture3("which", "diff")
        return status.success?
    end
end


def fallbackDiff(expectedPath, outputPath)
    # Fallback diff for when diff(1) isn't available
    diffs = []
    line_number = 0
    File.open(expectedPath) { |expected|
        File.open(outputPath) { |actual|
            loop {
                l1 = expected.gets
                l2 = actual.gets
                if (l1 != l2)
                    diffs.push([line_number, l1, l2])
                end
                line_number = line_number + 1
                break if (l1 == nil && l2 == nil)
            }
        }
    }
    if diffs.empty?
        return ""
    end
    diffOut = diffs.map { |diff|
        "@@ -\#{diff[0]},1 +\#{diff[0]},1 @@\\n" +
            (diff[1] ? "-\#{diff[1]}" : "") +
            (diff[2] ? "+\#{diff[2]}" : "")
    }.join("")
    return diffOut
end

def runDiff(expectedPath, outputPath)
    diffOut, diffStatus = Open3.capture2("diff",
                                         "--strip-trailing-cr",
                                         "-u",
                                         expectedPath.to_s,
                                         outputPath.to_s)
    if diffStatus.success?
        return ""
    else
        return diffOut
    end
end

def getDiff(expected, output)
    if hasDiff
        return runDiff(expected, output)
    end
    return fallbackDiff(expected, output)
end

# Prefix each line of str with the name
def prefixString(str, name)
    str.empty? ? "" : str.gsub(/^/m, "#{name}: ")
end

def silentOutputHandler(out, err, name)
    out = out + err
    $stdout.puts(prefixString(out, name)) if not out.empty?
    outpath = Pathname.new("..") + (name + ".out").to_s
    File.open(outpath, "w") { |o|
        o.write(out)
        o.flush
    }
    outpath
end

def noisyOutputHandler(out, err, name)
    out = out + err
    outpath = Pathname.new("..") + (name + ".out").to_s
    File.open(outpath, "w") { |o|
        o.write(out)
        o.flush
    }
    outpath
end

class ErrorHandlers
    def initialize(name, statusFile, runID, verbosity, progressMeter, reportExecutionTime)
        @name = name
        @statusFile = statusFile
        @runID = runID
        @verbosity = verbosity
        @progressMeter = progressMeter
        @reportExecutionTime = reportExecutionTime
    end
    def simpleErrorHandler(exitStatus, _outpath, expected)
        raise "Can't get here" unless expected.nil?
        if not exitStatus.success?
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        else
            successCommand(exitStatus)
        end
    end
    def expectedFailErrorHandler(exitStatus, _outpath, expected)
        raise "Can't get here" unless expected.nil?
        if exitStatus.success?
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        else
            successCommand(exitStatus)
        end
    end
    def noisyErrorHandler(exitStatus, outpath, expected)
        raise "Can't get here" unless expected.nil?
        if exitStatus.success?
            successCommand(exitStatus)
        else
            out = IO.read(outpath)
            pp(out)
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        end
    end
    def diffErrorHandler(exitStatus, outpath, expected)
        expected = Pathname.new("..") + expected
        if not exitStatus.success?
            out = IO.read(outpath)
            pp(out)
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        elsif expected.file?
            ret = getDiff(expected, outpath)
            if ret.size == 0
                successCommand(exitStatus)
            else
                pp("DIFF FAILURE!")
                pp(ret)
                failCommand(exitStatus)
            end
        else
            pp("NO EXPECTATION!")
            pp(IO.read(outpath))
            failCommand(exitStatus)
        end
    end
    def mozillaErrorHandler(exitStatus, outpath, expected)
        raise "Can't get here" unless expected.nil?
        out = IO.read(outpath)
        if not exitStatus.success?
            pp(out)
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        elsif /failed!/i =~ out
            pp("Detected failures:")
            pp(out)
            failCommand(exitStatus)
        else
            successCommand(exitStatus)
        end
    end
    def mozillaFailErrorHandler(exitStatus, outpath, expected)
        raise "Can't get here" unless expected.nil?
        out = IO.read(outpath)
        if not exitStatus.success?
            successCommand(exitStatus)
        elsif /failed!/i =~ out
            successCommand(exitStatus)
        else
            pp("NOTICE: You made this test pass, but it was expected to fail")
            failCommand(exitStatus)
        end
    end
    def mozillaExit3ErrorHandler(exitStatus, outpath, expected)
        raise "Can't get here" unless expected.nil?
        out = IO.read(outpath)
        if exitStatus.success?
            pp(out)
            pp("ERROR: Test expected to fail, but returned successfully")
            failCommand(exitStatus)
        elsif exitStatus.exitstatus != 3
            pp(out)
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        elsif /failed!/i =~ out
            pp("Detected failures:")
            pp(out)
            failCommand(exitStatus)
        else
            successCommand(exitStatus)
        end
    end
    def chakraPassFailErrorHandler(exitStatus, outpath, expected)
        raise "Can't get here" unless expected.nil?
        out = IO.read(outpath)
        if not exitStatus.success?
            pp(out)
            unexpectedExitCode(exitStatus)
            failCommand(exitStatus)
        elsif /FAILED/i =~ out
            pp("Detected failures:")
            pp(out)
            failCommand(exitStatus)
        else
            successCommand(exitStatus)
        end
    end
    def rescueErrorHandler
        failCommand(nil)
    end
    private
    def pp(s) # put prefixed
        return unless s.size > 0
        puts(prefixString(s, @name))
    end
    def unexpectedExitCode(exitStatus)
        pp("ERROR: Unexpected exit code: #{exitStatus.exitstatus}")
    end
    def statusCommand(exitStatus, statusCode)
        # May be called in the rescue block, so status is not
        # guaranteed to be set; if it isn't, set the exit code to
        # something that's clearly invalid.

        File.open("#{@statusFile}", "w") { |f|
            status = exitStatus.nil? ? 999999999 : (exitStatus.exited? ? exitStatus.exitstatus : 88888888)
            f.puts("#{@runID} #{status} #{statusCode}")
        }
    end
    def failCommand(exitStatus)
        puts("FAIL: #{@name}")
        statusCommand(exitStatus, STATUS_FILE_FAIL)
    end
    def successCommand(exitStatus)
        if @progressMeter or @verbosity >= 2 # XXX
            puts("PASS: #{@name}")
        end
        statusCommand(exitStatus, STATUS_FILE_PASS)
    end
end

$outputHandlers = {
    "silent" => method(:silentOutputHandler),
    "noisy" => method(:noisyOutputHandler),
}

def runTest(index, vars, runID, verbosity, progressMeter, reportExecutionTime)
    name = vars["TESTNAME"]
    puts("Running #{name}")
    statusFile = "#{STATUS_FILE_PREFIX}#{index}"
    errorHandlers = ErrorHandlers.new(name, statusFile, runID, verbosity, progressMeter, reportExecutionTime)
    begin
        require 'open3'
        env = envVars(vars)
        if verbosity >= 3
            print(vars["CMD"])
        end
        cmd = Shellwords.shellwords(vars["CMD"])
        out, err, status = Open3.capture3(env, *cmd, :chdir => "../#{vars["DIRECTORY"]}")
        outpath = $outputHandlers[vars["OUTPUT_HANDLER"]].call(out, err, name)
        expected = nil
        errh = vars["ERROR_HANDLER"]
        md = /^diff (.*)/.match(errh)
        if not md.nil?
            expected = md[1]
            errh = "diff"
        end
        errorHandlers.send("#{errh}ErrorHandler".to_sym, status, outpath, expected)
    rescue
        puts "FAIL: #{name}"
        errorHandlers.rescueErrorHandler
        raise
    end
end

def cmdTest(testcase, vars)
    verbosity = 0
    progressMeter = false
    reportExecutionTime = false

    runID = nil
    ARGV.each { |arg|
        case arg
        when "-v"
            verbosity += 1
        when "-p"
            progressMeter = true
        when "-r"
            reportExecutionTime = true
        else
            runID = arg
        end
    }

    if runID.nil?
        $stderr.puts("Need a runID")
        exit(3)
    end

    md = /.*_(\d+)$/.match(testcase)
    if md.nil?
        raise "Cannot determine index from `#{testcase}`"
    end

    Dir.chdir(Pathname.new(testcase).dirname) {
        runTest(md[1], vars, runID, verbosity, progressMeter, reportExecutionTime)
    }
end

def cmdRun(testcase, vars)
    env = envVars(vars)
    cmd = Shellwords.shellwords(vars["CMD"]) + ARGV
    Dir.chdir(Pathname.new(testcase).dirname) {
        Process.exec(env, *cmd, :chdir => "../#{vars["DIRECTORY"]}")
    }
end

def usage
    puts("Usage:")
    puts("#{$0} testcase test|run")
end

if ARGV.size < 2
    $stderr.puts("Need at least 3 arguments, got #{ARGV.size} #{ARGV}")
    usage
    exit(2)
end

testcase = ARGV.shift
vars = loadVars(testcase)

action = ARGV.shift

case action
when "test"
    cmdTest(testcase, vars)
when "run"
    cmdRun(testcase, vars)
else
    $stderr.puts("Unrecognized action: #{action}")
    exit(3)
end

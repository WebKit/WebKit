# Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

$hasDiff = false
if ($hostOS == 'windows' || $hostOS == 'playstation')
    out, err, status = Open3.capture3("where", "/q", "diff")
    $hasDiff = status.success?
else
    out, err, status = Open3.capture3("which", "diff")
    $hasDiff = status.success?
end

# Prefix each line of str with the name
def prefixString(str, name)
    "#{str}.empty? ? \"\" : #{str}.gsub(/^/m, \"#{name}: \")"
end

def silentOutputHandler
    Proc.new {
        | name |
        <<-END_SILENT_OUTPUT_HANDLER
        out = out + err
        err = nil
        STDOUT.puts #{prefixString("out", name)} if (!out.empty?)
        File.open("#{Shellwords.shellescape((Pathname("..") + (name + ".out")).to_s)}", "w") do |out_file|
            out_file.puts out
        end
        END_SILENT_OUTPUT_HANDLER
    }
end

# Output handler for tests that are expected to produce meaningful output.
def noisyOutputHandler
    Proc.new {
        | name |
        <<-END_NOISY_OUTPUT_HANDLER
        out = out + err
        err = nil
        File.open("#{Shellwords.shellescape((Pathname("..") + (name + ".out")).to_s)}", "w") do |out_file|
            out_file.puts out
        end
        END_NOISY_OUTPUT_HANDLER
    }
end

# Error handler for tests that fail exactly when they return non-zero exit status.
# This is useful when a test is expected to fail.
def simpleErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if !success\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status}\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
    }
end

# Error handler for tests that fail exactly when they return zero exit status.
def expectedFailErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if success\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code 0\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
    }
end

# Error handler for tests that fail exactly when they return non-zero exit status and produce
# lots of spew. This will echo that spew when the test fails.
def noisyErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if !success\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status}\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
    }
end

 def fallbackDiff(expected, output)
    <<-END_FALLBACK_DIFF
         # Fallback diff for when diff(1) isn't available
         diffs = []
         line_number = 0
         File.open("#{expected}") do | expected |
             File.open("#{output}") do | actual |
                 loop do
                     l1 = expected.gets
                     l2 = actual.gets
                     if (l1 != l2)
                         diffs.push([line_number, l1, l2])
                     end
                     line_number = line_number + 1
                     break if (l1 == nil && l2 == nil)
                 end
             end
         end
         isDifferent = !diffs.empty?
         diffOut = diffs.map {
             | diff |
             "@@ -\#{diff[0]},1 +\#{diff[0]},1 @@\\n" +
             (diff[1] ? "-\#{diff[1]}" : "") +
             (diff[2] ? "+\#{diff[2]}" : "")
        }.join("")
    END_FALLBACK_DIFF
end

def runDiff(expected, output)
    <<-END_RUN_DIFF
        diffOut, diffStatus = Open3.capture2("diff",
            "--strip-trailing-cr",
            "-u",
            "#{expected}",
            "#{output}");
        isDifferent = !diffStatus.success?
    END_RUN_DIFF
end

# Get a difference between two files, using diff where available, falling back
# on a limited comparison when diff is not available
def getDiff(expected, output)
    if $hasDiff
        runDiff(expected, output)
    else
        fallbackDiff(expected,output)
    end
end

# Error handler for tests that diff their output with some expectation.
def diffErrorHandler(expectedFilename)
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts "if !success\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status}\\n\"", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "elsif File.exists?(\"../#{Shellwords.shellescape(expectedFilename)}\")\n"
        outp.puts getDiff("../#{Shellwords.shellescape(expectedFilename)}", outputFilename)
        outp.puts "    if isDifferent\n"
        outp.puts "        print " + prefixString("\"DIFF FAILURE!\\n\"", plan.name) + "\n"
        outp.puts "        print " + prefixString("diffOut", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "    else"
        outp.puts "        " + plan.successCommand
        outp.puts "    end"
        outp.puts "else\n"
        outp.puts "    print " + prefixString("\"NO EXPECTATION!\\n\"", plan.name) + "\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "end"
    }
end

# Error handler for tests that report error by saying "failed!". This is used by Mozilla
# tests.
def mozillaErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if !success\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status}\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "elsif /failed!/i =~ out\n"
        outp.puts "    print " + prefixString("\"Detected failures:\\n\"", plan.name) + "\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
    }
end

# Error handler for tests that report error by saying "failed!", and are expected to
# fail. This is used by Mozilla tests.
def mozillaFailErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if !success\n"
        outp.puts "    " + plan.successCommand
        outp.puts "elsif /failed!/i =~ out\n"
        outp.puts "    " + plan.successCommand
        outp.puts "else\n"
        outp.puts "    print " + prefixString("\"NOTICE: You made this test pass, but it was expected to fail\\n\"", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "end\n"
    }
end

# Error handler for tests that report error by saying "failed!", and are expected to have
# an exit code of 3.
def mozillaExit3ErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if success\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Test expected to fail, but returned successfully\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "elsif status != 3"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code: \#{status}\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "elsif /failed!/i =~ out\n"
        outp.puts "    print " + prefixString("\"Detected failures:\\n\"", plan.name) + "\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
    }
end

# Error handler for tests that report success by saying "Passed" or error by saying "FAILED".
# This is used by Chakra tests.
def chakraPassFailErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if !success\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status}\\n\"", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "elsif /FAILED/i =~ out\n"
        outp.puts "    print " + prefixString("\"Detected failures:\\n\"", plan.name) + "\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    " + plan.failCommand
        outp.puts "else\n"
        outp.puts "    " + plan.successCommand
        outp.puts "end\n"
   }
end

class Plan
    attr_reader :directory, :arguments, :family, :name, :outputHandler, :errorHandler, :additionalEnv
    attr_accessor :index

    def initialize(directory, arguments, family, name, outputHandler, errorHandler)
        @directory = directory
        @arguments = arguments[1..-1]
        @family = family
        @name = name
        @outputHandler = outputHandler
        @errorHandler = errorHandler
        @isSlow = !!$runCommandOptions[:isSlow]
        @additionalEnv = []
    end

    def shellCommand
        n = @name.gsub(/(\\|\/)/, '_')
        script = "out = nil\n"
        script += "err = nil\n"
        script += "status = -1\n";
        script += "success = false\n"
        script += "File.open(\"#{n}-exit\", \"r\") do |stat|\n";
        script += "  status = stat.gets.to_i\n"
        script += "end\n"
        script += "if (status == 0)\n"
        script += "  success = true\n"
        script += "end\n"

        script += "out = File.open(\"#{n}-stdout\", \"r\").read\n"
        script += "err = File.open(\"#{n}-stderr\", \"r\").read\n"
        return script
    end

    def reproScriptHelper
        return ""
    end

    def reproScriptCommand
        return ""
    end

    def failCommand
        <<-END_FAIL_COMMAND
            print "FAIL: #{Shellwords.shellescape(@name)}\n"
            FileUtils.touch("#{failFile}")
            #{reproScriptCommand}
        END_FAIL_COMMAND
    end

    def successCommand
        if $progressMeter or $verbosity >= 2
            <<-END_VERBOSE_SUCCESS_COMMAND
                File.unlink("#{failFile}") if File.exists?("#{failFile}")
                print "PASS: #{Shellwords.shellescape(@name)}\n"
            END_VERBOSE_SUCCESS_COMMAND
        else
            "File.unlink(\"#{failFile}\") if File.exists?(\"#{failFile}\")\n"
        end
    end

    def failFile
        "test_fail_#{@index}"
    end

    def statusWrite
        <<-END_STATUS_WRITE
            if !success
                File.open("#{failFile}", "w") do |code_file|
                    code_file.puts status
                end
            end
        END_STATUS_WRITE
    end

    def writeRunScript(filename)
        jsonfilepart = filename.basename.to_s + ".json"
        jsondir = filename.dirname.parent + ".json"
        if (!jsondir.exist?)
            jsondir.mkdir
        end
        File.open(jsondir + jsonfilepart, "w") {
            |outp|

            baseDir = ($runnerDir + ".." + @directory).realdirpath

            outp.puts JSON.generate({
                name: @name,
                outputDir: $runnerDir,
                baseDir: baseDir,
                env: $envVars + @additionalEnv,
                outputName: @name.gsub(/(\\|\/)/, '_'),
                checkScript: filename,
                args: @arguments,
                failFile: "#{failFile}"
            })
        }

        File.open(filename, "w") {
            | outp |
            #outp.puts "print \"Running #{Shellwords.shellescape(@name)}\\n\""
            outp.puts "#{reproScriptHelper}"
            outp.puts "begin"
            outp.puts "require 'open3'"
            outp.puts "require 'fileutils'"

            cmd = shellCommand

            cmd += statusWrite

            cmd += @outputHandler.call(@name)

            if $verbosity >= 3
                outp.puts "print \"#{Shellwords.shellescape(cmd)}\\n\""
            end
            outp.puts cmd
            @errorHandler.call(outp, self)
            outp.puts "rescue RuntimeError => e"
            outp.puts "    print \"FAIL: #{Shellwords.shellescape(@name)}\\n\""
            outp.puts "    FileUtils.touch(\"#{failFile}\")"
            outp.puts "end"
        }
    end
end

def preparePlayStationTestRunner
    File.open($runnerDir + "runscript", "w") {
        | outp |
        $runlist.each {
            | plan |
            outp.puts "../.json/test_script_#{plan.index}.json"
        }
    }
end

def prepareShellTestRunner
    preparePlayStationTestRunner
end

def prepareMakeTestRunner(remoteIndex)
    preparePlayStationTestRunner
end

def prepareRubyTestRunner
    preparePlayStationTestRunner
end

def testRunnerCommand
    options = ENV["JSCTEST_options"]

    fsRoot = $jscPath.dirname
    command = "PlayStationTestRunner #{options} -numProcesses=#{$numChildProcesses.to_s} -exe=#{$jscPath} -fsroot=#{fsRoot} @#{$runnerDir}/runscript"

    print command

    return command
end

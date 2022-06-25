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
if ($hostOS == 'windows')
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
        outp.puts "if !success(status)\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status.exitstatus}\\n\"", plan.name) + "\n"
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
        outp.puts "if success(status)\n"
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
        outp.puts "if !success(status)\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status.exitstatus}\\n\"", plan.name) + "\n"
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
        
        outp.puts "if !success(status)\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status.exitstatus}\\n\"", plan.name) + "\n"
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
        outp.puts "if !success(status)\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status.exitstatus}\\n\"", plan.name) + "\n"
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
        outp.puts "if !success(status)\n"
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
        outp.puts "if success(status)\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Test expected to fail, but returned successfully\\n\"", plan.name) + "\n"
        outp.puts "        " + plan.failCommand
        outp.puts "elsif status.exitstatus != 3"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code: \#{status.exitstatus}\\n\"", plan.name) + "\n"
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
        outp.puts "if !success(status)\n"
        outp.puts "    print " + prefixString("out", plan.name) + "\n"
        outp.puts "    print " + prefixString("\"ERROR: Unexpected exit code \#{status.exitstatus}\\n\"", plan.name) + "\n"
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

class Plan < BasePlan
    def shellCommand 
        script = "out = nil\n"
        script += "err = nil\n"
        script += "status = nil\n"
        script += "Dir.chdir(\"../#{Shellwords.shellescape(@directory.to_s)}\") do\n"
        script += "  env = {}\n"
        ($envVars + additionalEnv).each {
            |var| 
            (key, value) = var.split(/=/, 2)
            script += "  env[\"#{key}\"] = \"#{value}\"\n"
        }
        script += "    out, err, status = Open3.capture3(env, \n"
        script += @arguments.map { | argument | "        \"#{argument}\""}.join(",\n")
        script += "    )\n"
        script += "end\n"
        return script
    end

    def reproScriptHelper
        script = "def error_script_contents\n"
        script += "    <<-END_OF_SCRIPT\n"
        script += "        require 'open3'\n"
        script += "        def success(status)\n"
        script += "            status.success?\n"
        script += "        end\n"
        script += "        script_location = File.expand_path(File.dirname(__FILE__))\n"
        script += "        Dir.chdir(\"\\\#{script_location}"
        Pathname.new(@name).dirname.each_filename {
            | pathComponent |
            script += "/.."
        }
        script += "/.runner\") do\n"
        script += "            ENV[\"DYLD_FRAMEWORK_PATH\"] = \"#{$testingFrameworkPath.dirname}\"\n"
        script += "            ENV[\"JSCTEST_timeout\"] = \"#{ENV['JSCTEST_timeout']}\"\n"
        script += "            ENV[\"JSCTEST_hardTimeout\"] = \"#{ENV['JSCTEST_hardTimeout']}\"\n"
        script += "            ENV[\"JSCTEST_memoryLimit\"] = \"#{ENV['JSCTEST_memoryLimit']}\"\n"

        script += "            #{shellCommand}"
        script += "            print out\n"
        script += "            if (!success(status))\n"
        script += "                exit(1)\n"
        script += "            end\n"
        script += "        end\n"
        script += "    END_OF_SCRIPT\n"
        script += "end\n"
        return script
    end
    
    def reproScriptCommand
        <<-END_REPRO_SCRIPT_COMMAND
            File.open("#{Shellwords.shellescape((Pathname.new("..") + @name).to_s)}", "w") do |scr|
                scr.puts "\#{error_script_contents}"
            end
        END_REPRO_SCRIPT_COMMAND
    end

    def statusCommand(status_code)
        # May be called in th rescue block, so status is not
        # guaranteed to be set; if it isn't, set the exit code to
        # something that's clearly invalid.
        <<-END_STATUS_COMMAND
          File.open("#{statusFile}", "w") { |f|
              f.puts("#{$runUniqueId} \#{status.nil? ? 999999999 : status.exitstatus} #{status_code}")
          }
        END_STATUS_COMMAND
    end

    def failCommand
        <<-END_FAIL_COMMAND
            print "FAIL: #{Shellwords.shellescape(@name)}\n"
            #{statusCommand(STATUS_FILE_FAIL)}
            #{reproScriptCommand}
        END_FAIL_COMMAND
    end
    
    def successCommand
        if $progressMeter or $verbosity >= 2
            <<-END_VERBOSE_SUCCESS_COMMAND
                print "PASS: #{Shellwords.shellescape(@name)}\n"
                #{statusCommand(STATUS_FILE_PASS)}
            END_VERBOSE_SUCCESS_COMMAND
        else
            "#{statusCommand(STATUS_FILE_PASS)}\n"
        end
    end
    
    def statusFile
        "#{STATUS_FILE_PREFIX}#{@index}"
    end

    def writeRunScript(filename)
        File.open(filename, "w") {
            | outp |
            outp.puts "print \"Running #{Shellwords.shellescape(@name)}\\n\""
            outp.puts "#{reproScriptHelper}"
            outp.puts "begin"
            outp.puts "require 'open3'"
            outp.puts "require 'fileutils'"
            outp.puts "def success(status)"
            outp.puts "   status.success?"
            outp.puts "end"

            cmd = shellCommand

            cmd += @outputHandler.call(@name)

            if $verbosity >= 3
                outp.puts "print \"#{Shellwords.shellescape(cmd)}\\n\""
            end
            outp.puts cmd
            @errorHandler.call(outp, self)
            outp.puts "rescue"
            outp.puts "    print \"FAIL: #{Shellwords.shellescape(@name)}\\n\""
            outp.puts "    #{statusCommand(STATUS_FILE_FAIL)}"
            outp.puts "end"
        }
    end
end

class TestRunnerShell < TestRunner
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        File.open("#{@runnerDir + "runscript"}", "w") { |f|
            runlist.each { |plan|
                if completedPlans.include?(plan)
                    next
                end
                f.puts("ruby test_script_#{plan.index}")
            }
        }
        `dos2unix #{@runnerDir + "runscript"}`
    end
    def command(remoteIndex=0)
        "sh runscript"
    end
end

class TestRunnerMake < TestRunner
    def output_target(outp, plan, prereqs)
        index = plan.index
        target = "test_done_#{index}"
        outp.puts "#{target}: #{prereqs.join(" ")}"
        outp.puts "\truby test_script_#{index}"
        target
    end
    def prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteIndex)
        runPlans = []
        serialRunPlans = []
        runlist.each {
            | plan |
            if completedPlans.include?(plan)
                next
            end
            if @remoteHosts.nil? or plan.index % @remoteHosts.length == remoteIndex
                if serialPlans.include?(plan)
                    serialRunPlans << plan
                else
                    runPlans << plan
                end
            end
        }

        File.open(@runnerDir + "Makefile.#{remoteIndex}", "w") {
            | outp |
            if serialRunPlans.empty?
                outp.puts("all: parallel")
            else
                serialPrereq = "test_done_#{serialRunPlans[-1].index}"
                outp.puts("all: #{serialPrereq}")
                prev_target = "parallel"
                serialRunPlans.each {
                    | plan |
                    prev_target = output_target(outp, plan, [prev_target])
                }
            end
            parallelTargets = runPlans.collect {
                | plan |
                output_target(outp, plan, [])
            }
            outp.puts("parallel: " + parallelTargets.join(" "))
        }
    end
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        if remoteHosts.nil?
            prepareRunnerForRemote(runlist, serialPlans, completedPlans, 0)
        else
            remoteHosts.each_index {
                |remoteIndex|
                prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteIndex)
            }
        end
    end
    def command(remoteIndex=0)
        "make -j #{$numChildProcesses} -s -f Makefile.#{remoteIndex}"
    end
end

class TestRunnerRuby < TestRunner
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        File.open(@runnerDir + "runscript", "w") {
            | outp |
            runlist.each {
                | plan |
                if completedPlans.include?(plan)
                    next
                end
                outp.puts "system \"ruby test_script_#{plan.index}\""
            }
        }
    end
    def command(remoteIndex=0)
        "ruby runscript"
    end
end

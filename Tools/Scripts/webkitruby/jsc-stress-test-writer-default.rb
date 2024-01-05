# Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

def prefixCommand(prefix)
    "awk " + Shellwords.shellescape("{ printf #{(prefix + ': ').inspect}; print }")
end

def redirectAndPrefixCommand(prefix)
    prefixCommand(prefix) + " 2>&1"
end

def pipeAndPrefixCommand(outputFilename, prefix)
    "tee " + Shellwords.shellescape(outputFilename.to_s) + " | " + prefixCommand(prefix)
end

# Output handler for tests that are expected to be silent.
def silentOutputHandler
    Proc.new {
        | name |
        pipeAndPrefixCommand((Pathname("..") + (name + ".out")).to_s, name)
    }
end

# Output handler for tests that are expected to produce meaningful output.
def noisyOutputHandler
    Proc.new {
        | name |
        "cat > " + Shellwords.shellescape((Pathname("..") + (name + ".out")).to_s)
    }
end

def getAndTestExitCode(plan, condition)
    <<-EOF
    if test "$exitCode" #{condition}
EOF
end

# Error handler for tests that fail exactly when they return non-zero exit status.
# This is useful when a test is expected to fail.
def simpleErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    (echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "else"
        outp.puts "    " + plan.successCommand
        outp.puts "fi"
    }
end

# Error handler for tests that fail exactly when they return zero exit status.
def expectedFailErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    " + plan.successCommand
        outp.puts "else"
        outp.puts "    (echo ERROR: Unexpected exit code: 0) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "fi"
    }
end

# Error handler for tests that fail exactly when they return non-zero exit status and produce
# lots of spew. This will echo that spew when the test fails.
def noisyErrorHandler
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "else"
        outp.puts "    " + plan.successCommand
        outp.puts "fi"
    }
end

# Error handler for tests that diff their output with some expectation.
def diffErrorHandler(expectedFilename)
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)
        diffFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".diff")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "elif test -e ../#{Shellwords.shellescape(expectedFilename)}"
        outp.puts "then"
        outp.puts "    diff --strip-trailing-cr -u ../#{Shellwords.shellescape(expectedFilename)} #{outputFilename} > #{diffFilename}"
        outp.puts "    if [ $? -eq 0 ]"
        outp.puts "    then"
        outp.puts "    " + plan.successCommand
        outp.puts "    else"
        outp.puts "        (echo \"DIFF FAILURE!\" && cat #{diffFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "        " + plan.failCommand
        outp.puts "    fi"
        outp.puts "else"
        outp.puts "    (echo \"NO EXPECTATION!\" && cat #{outputFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "fi"
    }
end

# Error handler for tests that report error by saying "failed!". This is used by Mozilla
# tests.
def mozillaErrorHandler
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "elif grep -i -q failed! #{outputFilename}"
        outp.puts "then"
        outp.puts "    (echo Detected failures: && cat #{outputFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "else"
        outp.puts "    " + plan.successCommand
        outp.puts "fi"
    }
end

# Error handler for tests that report error by saying "failed!", and are expected to
# fail. This is used by Mozilla tests.
def mozillaFailErrorHandler
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    " + plan.successCommand
        outp.puts "elif grep -i -q failed! #{outputFilename}"
        outp.puts "then"
        outp.puts "    " + plan.successCommand
        outp.puts "else"
        outp.puts "    (echo NOTICE: You made this test pass, but it was expected to fail) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "fi"
    }
end

# Error handler for tests that report error by saying "failed!", and are expected to have
# an exit code of 3.
def mozillaExit3ErrorHandler
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    if [ \"$exitCode\" -eq 3 ]"
        outp.puts "    then"
        outp.puts "        if grep -i -q failed! #{outputFilename}"
        outp.puts "        then"
        outp.puts "            (echo Detected failures: && cat #{outputFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "            " + plan.failCommand
        outp.puts "        else"
        outp.puts "            " + plan.successCommand
        outp.puts "        fi"
        outp.puts "    else"
        outp.puts "        (cat #{outputFilename} && echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "        " + plan.failCommand
        outp.puts "    fi"
        outp.puts "else"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Test expected to fail, but returned successfully) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "fi"
    }
end

# Error handler for tests that report success by saying "Passed" or error by saying "FAILED".
# This is used by Chakra tests.
def chakraPassFailErrorHandler
    Proc.new {
        | outp, plan |
        outputFilename = Shellwords.shellescape((Pathname("..") + (plan.name + ".out")).to_s)

        outp.puts getAndTestExitCode(plan, "-ne 0")
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: $exitCode) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "elif grep -i -q FAILED #{outputFilename}"
        outp.puts "then"
        outp.puts "    (echo Detected failures: && cat #{outputFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "    " + plan.failCommand
        outp.puts "else"
        outp.puts "    " + plan.successCommand
        outp.puts "fi"
    }
end

class Plan < BasePlan
    def shellCommand
        # It's important to remember that the test is actually run in a subshell, so if we change directory
        # in the subshell when we return we will be in our original directory. This is nice because we don't
        # have to bend over backwards to do things relative to the root.
        script = "(cd ../#{Shellwords.shellescape(@directory.to_s)} && ("
        ($envVars + additionalEnv).each { |var| script += "export " << var << "; " }
        script += "\"$@\" " + escapeAll(@arguments) + "))"
        return script
    end
    
    def reproScriptCommand
        # We have to find our way back to the .runner directory since that's where all of the relative
        # paths assume they start out from.
        script = "CURRENT_DIR=\"$( cd \"$( dirname \"${BASH_SOURCE[0]}\" )\" && pwd )\"\n"
        script += "cd $CURRENT_DIR\n"
        Pathname.new(@name).dirname.each_filename {
            | pathComponent |
            script += "cd ..\n"
        }
        script += "cd .runner\n"

        script += "export DYLD_FRAMEWORK_PATH=$(cd #{$testingFrameworkPath.dirname}; pwd)\n"
        script += "export JSCTEST_timeout=#{Shellwords.shellescape(ENV['JSCTEST_timeout'])}\n"
        script += "export JSCTEST_hardTimeout=#{Shellwords.shellescape(ENV['JSCTEST_hardTimeout'])}\n"
        script += "export JSCTEST_memoryLimit=#{Shellwords.shellescape(ENV['JSCTEST_memoryLimit'])}\n"
        $envVars.each { |var| script += "export " << var << "\n" }
        script += "#{shellCommand} || exit 1"
        "echo #{Shellwords.shellescape(script)} > #{Shellwords.shellescape((Pathname.new("..") + @name).to_s)}"
    end

    def statusCommand(status)
        "echo #{@index} #{$runUniqueId} $exitCode #{status} >> #{statusFile}"
    end

    def failCommand
        "#{statusCommand(STATUS_FILE_FAIL)}; echo FAIL: #{Shellwords.shellescape(@name)}; " + reproScriptCommand
    end
    
    def successCommand
        command = ""
        executionTimeMessage = ""
        if $reportExecutionTime
            executionTimeMessage = " $(($SECONDS - $START_TIME))s"
        end
        if $progressMeter or $reportExecutionTime or $verbosity >= 2
            command = "echo PASS: #{Shellwords.shellescape(@name)}#{executionTimeMessage}"
        end
        "#{statusCommand(STATUS_FILE_PASS)}; #{command}"
    end
    
    def statusFile
        "#{STATUS_FILE}"
    end
    
    def writeRunScript(filename)
        File.open(filename, "w") {
            | outp |
            if $reportExecutionTime
                outp.puts "START_TIME=$SECONDS"
            end
            outp.puts "echo Running #{Shellwords.shellescape(@name)}"
            #
            # +--------------------------------------------------------------------+
            # | +-----------------------------------------------+                  |
            # | | +--------------+     +-------------------+    |                  |
            # | | | cmd 1 ----> 1|---> |0 --> outH 1 ---> 4|-> 4|---------------> 1|
            # | | |     2 /      |     +-------------------+    |   +-----------+  |
            # | | |echo $? 0 -> 3|---------------------------> 1|-> |0 read xs  |  |
            # | | +--------------+                              |   |  exit $xs |  |
            # | |                                               |   +-----------+  |
            # | +-----------------------------------------------+                  |
            # +--------------------------------------------------------------------+
            # From the top down (i.e. reading from the outer expression inwards):
            #
            # - Redirect FD 4 to our stdout
            #
            # - Build a pipe of two command sequences. The
            #   right-hand-side sequence reads a number from stdin and
            #   exits with it. Since it's the last command in the
            #   pipeline, this will be the value of $? after the
            #   pipeline completes.
            #
            # - In the left-hand-side sequence, redirect FD 3 to FD 1.
            #
            # - Build a pipe of two commands
            #   - run shellCommand, writing its exit code to FD 3.
            #   - run the outputHandler, with its stdin coming from
            #     the pipe, redirecting its output to FD 4. The
            #     outputHandler needs to be in a command sequence
            #     (i.e. in { cmd; ...}) as it may do its own
            #     redirections.
            #
            # We do all this
            # - to avoid having to use a temporary file for the exit code
            # - to keep within the bounds of POSIX sh (i.e. can't use
            #   PIPESTATUS)
            cmd = "{ { { { #{shellCommand} 2>&1; echo $? >&3; } | { #{outputHandler.call(@name)} ;} >&4; } 3>&1; } | { read xs; exit $xs; } } 4>&1\nexitCode=$?\n"
            if $verbosity >= 3
                outp.puts "echo #{Shellwords.shellescape(cmd)}"
            end
            outp.puts cmd
            @errorHandler.call(outp, self)
        }
    end
end

class TestRunnerShell < TestRunner
    def initialize(testRunnerType, runnerDir)
        super(testRunnerType, runnerDir)
        @testListPath = runnerDir + 'testlist'
    end
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        FileUtils.cp SCRIPTS_PATH + "jsc-stress-test-helpers" + "shell-runner.sh", @runnerDir + "runscript"
        File.open(@testListPath, "w") { |f|
            runlist.each { |plan|
                if completedPlans.include?(plan)
                    next
                end
                f.puts("test_script_#{plan.index}")
            }
        }
    end
    def command(remoteIndex=0)
        "sh runscript #{File.basename(@testListPath)}"
    end
end

class TestRunnerMake < TestRunner
    def output_target(outp, plan, prereqs)
        index = plan.index
        target = "test_done_#{index}"
        outp.puts "#{target}: #{prereqs.join(" ")}"
        outp.puts "\tsh test_script_#{index}"
        target
    end
    def prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteHosts, remoteIndex)
        # The goals of our parallel test runner are scalability and simplicity. The
        # simplicity part is particularly important. We don't want to have to have
        # a full-time contributor just philosophising about parallel testing.
        #
        # As such, we just pass off all of the hard work to 'make'. This
        # creates a dummy directory ("$outputDir/.runner") in which we
        # create a dummy Makefile. The Makefile has a 'parallel' rule that
        # depends all tests, other than the ones marked 'serial'. The
        # serial tests are arranged in a chain; the last target in the
        # serial chain depends on 'parallel' and 'all' depends on the head
        # of the chain. Running 'make -j <whatever>' on this Makefile
        # results in 'make' doing all of the hard work:
        #
        # - Load balancing just works. Most systems have a great load balancer in
        #   'make'. If your system doesn't then just install a real 'make'.
        #
        # - Interruptions just work. For example Ctrl-C handling in 'make' is
        #   exactly right. You don't have to worry about zombie processes.
        #
        # We then do some tricks to make failure detection work and to make this
        # totally sound. If a test fails, we don't want the whole 'make' job to
        # stop. We also don't have any facility for makefile-escaping of path names.
        # We do have such a thing for shell-escaping, though. We fix both problems
        # by having the actual work for each of the test rules be done in a shell
        # script on the side. There is one such script per test. The script responds
        # to failure by printing something on the console and then touching a
        # failure file for that test, but then still returns 0. This makes 'make'
        # continue past that failure and complete all the tests anyway.
        #
        # In the end, this script collects all of the failures by searching for
        # files in the .runner directory whose name matches /^test_fail_/, where
        # the thing after the 'fail_' is the test index. Those are the files that
        # would be created by the test scripts if they detect failure. We're
        # basically using the filesystem as a concurrent database of test failures.
        # Even if two tests fail at the same time, since they're touching different
        # files we won't miss any failures.
        runPlans = []
        serialRunPlans = []
        runlist.each {
            | plan |
            if completedPlans.include?(plan)
                next
            end
            if remoteHosts.nil? or plan.index % remoteHosts.length == remoteIndex
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
            prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteHosts, 0)
        else
            remoteHosts.each_index {
                |remoteIndex|
                prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteHosts, remoteIndex)
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
                outp.puts "print `sh test_script_#{plan.index} 2>&1`"
            }
        }
    end
    def command(remoteIndex=0)
        "ruby runscript"
    end
end

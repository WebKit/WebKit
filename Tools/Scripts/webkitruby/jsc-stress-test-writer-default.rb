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
        " | " + pipeAndPrefixCommand((Pathname("..") + (name + ".out")).to_s, name)
    }
end

# Output handler for tests that are expected to produce meaningful output.
def noisyOutputHandler
    Proc.new {
        | name |
        " | cat > " + Shellwords.shellescape((Pathname("..") + (name + ".out")).to_s)
    }
end

# Error handler for tests that fail exactly when they return non-zero exit status.
# This is useful when a test is expected to fail.
def simpleErrorHandler
    Proc.new {
        | outp, plan |
        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    (echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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
        outp.puts "if test -e #{plan.failFile}"
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
    
        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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
        
        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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

        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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

        outp.puts "if test -e #{plan.failFile}"
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

        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    if [ `cat #{plan.failFile}` -eq 3 ]"
        outp.puts "    then"
        outp.puts "        if grep -i -q failed! #{outputFilename}"
        outp.puts "        then"
        outp.puts "            (echo Detected failures: && cat #{outputFilename}) | " + redirectAndPrefixCommand(plan.name)
        outp.puts "            " + plan.failCommand
        outp.puts "        else"
        outp.puts "            " + plan.successCommand
        outp.puts "        fi"
        outp.puts "    else"
        outp.puts "        (cat #{outputFilename} && echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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

        outp.puts "if test -e #{plan.failFile}"
        outp.puts "then"
        outp.puts "    (cat #{outputFilename} && echo ERROR: Unexpected exit code: `cat #{plan.failFile}`) | " + redirectAndPrefixCommand(plan.name)
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

class Plan
    attr_reader :directory, :arguments, :family, :name, :outputHandler, :errorHandler, :additionalEnv
    attr_accessor :index
    
    def initialize(directory, arguments, family, name, outputHandler, errorHandler)
        @directory = directory
        @arguments = arguments
        @family = family
        @name = name
        @outputHandler = outputHandler
        @errorHandler = errorHandler
        @isSlow = !!$runCommandOptions[:isSlow]
        @shouldCrash = !!$runCommandOptions[:shouldCrash]
        if @shouldCrash
            @outputHandler = noisyOutputHandler
        end
        @additionalEnv = []
    end

    def timeoutMonitorAddon
        # In case the JSC timeout handler is not working, add a timeout monitor in the script
        interval = 30 # Seconds
        timeout = ENV['JSCTEST_timeout'].to_i + 10 # Let jsc timeout handler trigger first
        if !timeout
            timeout = interval
        end
        if timeout < interval
            interval = timeout
        end
        script = %Q[
(
    ((count_down = #{timeout}))
    while ((count_down > 0)); do
        if [ "$count_down" -gt #{interval} ]
        then
            sleep #{interval}
        else
            sleep count_down
        fi
        pgrep -P $$ 1>/dev/null || exit 0
        ((count_down -= #{interval}))
    done
    echo "#{Shellwords.shellescape(@name)} has timed out, killing with timeout monitor"
    kill -s SIGTERM $$
    sleep 5 
    pgrep -P 1>/dev/null $$ && kill -s SIGKILL $$
    echo FAIL: #{Shellwords.shellescape(@name)} ; touch #{failFile} ;
) &
]
        return script
    end

    def shellCommand
        # It's important to remember that the test is actually run in a subshell, so if we change directory
        # in the subshell when we return we will be in our original directory. This is nice because we don't
        # have to bend over backwards to do things relative to the root.
        script = "(cd ../#{Shellwords.shellescape(@directory.to_s)} && #{@shouldCrash ? "!" : ""}("
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
        $envVars.each { |var| script += "export " << var << "\n" }
        script += "#{shellCommand} || exit 1"
        "echo #{Shellwords.shellescape(script)} > #{Shellwords.shellescape((Pathname.new("..") + @name).to_s)}"
    end
    
    def failCommand
        "echo FAIL: #{Shellwords.shellescape(@name)} ; touch #{failFile} ; " + reproScriptCommand
    end
    
    def successCommand
        executionTimeMessage = ""
        if $reportExecutionTime
            executionTimeMessage = " $(($SECONDS - $START_TIME))s"
        end
        if $progressMeter or $reportExecutionTime or $verbosity >= 2
            "rm -f #{failFile} ; echo PASS: #{Shellwords.shellescape(@name)}#{executionTimeMessage}"
        else
            "rm -f #{failFile}"
        end
    end
    
    def failFile
        "test_fail_#{@index}"
    end
    
    def writeRunScript(filename)
        File.open(filename, "w") {
            | outp |
            if $reportExecutionTime
                outp.puts "START_TIME=$SECONDS"
            end
            outp.puts "echo Running #{Shellwords.shellescape(@name)}"
            cmd = timeoutMonitorAddon
            cmd += "(" + shellCommand + " || (echo $? > #{failFile})) 2>&1 "
            cmd += @outputHandler.call(@name)
            if $verbosity >= 3
                outp.puts "echo #{Shellwords.shellescape(cmd)}"
            end
            outp.puts cmd
            @errorHandler.call(outp, self)
        }
    end
end

def prepareShellTestRunner
    FileUtils.cp SCRIPTS_PATH + "jsc-stress-test-helpers" + "shell-runner.sh", $runnerDir + "runscript"
end

def prepareMakeTestRunner(remoteIndex)
    # The goals of our parallel test runner are scalability and simplicity. The
    # simplicity part is particularly important. We don't want to have to have
    # a full-time contributor just philosophising about parallel testing.
    #
    # As such, we just pass off all of the hard work to 'make'. This creates a
    # dummy directory ("$outputDir/.runner") in which we create a dummy
    # Makefile. The Makefile has an 'all' rule that depends on all of the tests.
    # That is, for each test we know we will run, there is a rule in the
    # Makefile and 'all' depends on it. Running 'make -j <whatever>' on this
    # Makefile results in 'make' doing all of the hard work:
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
    runIndices = []
    $runlist.each {
        | plan |
        if !$remote or plan.index % $remoteHosts.length == remoteIndex
            runIndices << plan.index
        end
    }
    
    File.open($runnerDir + "Makefile.#{remoteIndex}", "w") {
        | outp |
        outp.puts("all: " + runIndices.map{|v| "test_done_#{v}"}.join(' '))
        runIndices.each {
            | index |
            plan = $runlist[index]
            outp.puts "test_done_#{index}:"
            outp.puts "\tsh test_script_#{plan.index}"
        }
    }
end

def prepareRubyTestRunner
    File.open($runnerDir + "runscript", "w") {
        | outp |
        $runlist.each {
            | plan |
            outp.puts "print `sh test_script_#{plan.index} 2>&1`"
        }
    }
end

def testRunnerCommand(remoteIndex=0)
    case $testRunnerType
    when :shell
        command = "sh runscript"
    when :make
        command = "make -j #{$numChildProcesses} -s -f Makefile.#{remoteIndex}"
    when :ruby
        command = "ruby runscript"
    else
        raise "Unknown test runner type: #{$testRunnerType.to_s}"
    end
    return command
end

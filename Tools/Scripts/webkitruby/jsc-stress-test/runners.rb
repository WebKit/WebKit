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

class TestRunner
    def initialize(testRunnerType, runnerDir, testDispatcher)
        @testRunnerType = testRunnerType
        @runnerDir = runnerDir
        @testDispatcher = testDispatcher
    end
    def dispatcherCommand
        case @testDispatcher
        when "default"
            return "sh ../.helpers/dispatch-testcase"
        when "ruby"
            return "ruby ../.helpers/dispatch-testcase.rb"
        when "playstation"
            # The playstation runner does its own thing.
            raise "Can't get here"
        end
    end
    def prepare(runlist, serialPlans, completedPlans, remoteHosts)
        prepareScripts(runlist)
        prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
    end
    def prepareScripts(runlist)
        Dir.mkdir(@runnerDir) unless @runnerDir.directory?
        toDelete = []
        Dir.foreach(@runnerDir) {
            | filename |
            if filename =~ /^test_/
                toDelete << filename
            end
        }

        toDelete.each {
            | filename |
            File.unlink(@runnerDir + filename)
        }

        # Write test scripts in parallel as this is both an expensive and a
        # highly IO intensive operation, but each script is independent and
        # the operation is pure other than writing the unique run script.
        parallelEach(runlist) do | plan |
            plan.writeTestCase(@runnerDir + "testcase_#{plan.index}")
        end
    end
    def self.create(testRunnerType, runnerDir, testWriter)
        cls = nil
        case testRunnerType
        when :shell
            cls = TestRunnerShell
        when :make
            cls = TestRunnerMake
        when :ruby
            cls = TestRunnerRuby
        when :gnuparallel
            cls = TestRunnerGnuParallel
        else
            raise "Unknown test runner type: #{testRunnerType.to_s}"
        end
        if testWriter == "playstation"
            cls = TestRunnerPlaystation
        end
        return cls.new(testRunnerType, runnerDir, testWriter)
    end
end

class TestRunnerShell < TestRunner
    def initialize(testRunnerType, runnerDir, testDispatcher)
        super(testRunnerType, runnerDir, testDispatcher)
        @testListPath = runnerDir + 'testlist'
    end
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        FileUtils.cp SCRIPTS_PATH + "jsc-stress-test-helpers" + "shell-runner.sh", @runnerDir + "runscript"
        File.open(@testListPath, "w") { |f|
            runlist.each { |plan|
                if completedPlans.include?(plan)
                    next
                end
                f.puts("#{dispatcherCommand} ./testcase_#{plan.index} test $@ #{$runUniqueId}")
            }
        }
    end
    def command(remoteIndex=0)
        vs = (["-v"] * $verbosity).join(" ")
        ret = if $reportExecutionTime then " -r" else "" end
        pm = if $progressMeter then " -p" else "" end
        flags = "#{vs}#{ret}#{pm}"
        "sh runscript #{File.basename(@testListPath)} #{flags}"
    end
end

class TestRunnerMake < TestRunner
    def output_target(outp, plan, prereqs)
        index = plan.index
        target = "test_done_#{index}"
        outp.puts "#{target}: #{prereqs.join(" ")}"
        outp.puts "\t#{dispatcherCommand} ./testcase_#{index} test $(FLAGS) #{$runUniqueId}"
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
        runlist.each { |plan|
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

        File.open(@runnerDir + "Makefile.#{remoteIndex}", "w") { |outp|
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
            parallelTargets = runPlans.collect { |plan|
                output_target(outp, plan, [])
            }
            outp.puts("parallel: " + parallelTargets.join(" "))
        }
    end
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        if remoteHosts.nil?
            prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteHosts, 0)
        else
            remoteHosts.each_index { |remoteIndex|
                prepareRunnerForRemote(runlist, serialPlans, completedPlans, remoteHosts, remoteIndex)
            }
        end
    end
    def command(remoteIndex=0)
        vs = (["-v"] * $verbosity).join(" ")
        ret = if $reportExecutionTime then " -r" else "" end
        pm = if $progressMeter then " -p" else "" end
        flags = Shellwords.shellescape("FLAGS=#{vs}#{ret}#{pm}")
        "make -j #{$numChildProcesses} -s -f Makefile.#{remoteIndex} \"#{flags}\""
    end
end

class TestRunnerRuby < TestRunner
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        File.open(@runnerDir + "runscript", "w") { |outp|
            runlist.each { |plan|
                if completedPlans.include?(plan)
                    next
                end
                outp.puts "print `#{dispatcherCommand} ./testcase_#{plan.index} test \#{ARGV.join(' ')} #{$runUniqueId} 2>&1`"
            }
        }
    end
    def command(remoteIndex=0)
        vs = (["-v"] * $verbosity).join(" ")
        ret = if $reportExecutionTime then " -r" else "" end
        pm = if $progressMeter then " -p" else "" end
        flags = "#{vs}#{ret}#{pm}"
        "ruby runscript #{flags}"
    end
end

class TestRunnerPlaystation < TestRunner
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        File.open(@runnerDir + "runscript", "w") { |outp|
            runlist.each { |plan|
                if completedPlans.include?(plan)
                    next
                end
                outp.puts("./testcase_#{plan.index}")
            }
        }
    end
    def command(remoteIndex=0)
        options = ENV["JSCTEST_options"]

        fsRoot = $jscPath.dirname
        command = "PlayStationTestRunner #{options} -numProcesses=#{$numChildProcesses.to_s} -exe=#{$jscPath} -fsroot=#{fsRoot} --runUniqueId=#{$runUniqueId} @#{@runnerDir}/runscript"

        print command

        return command
    end
end

def each_chunk(e, chunkSize)
    accumulator = []
    e.each { |element|
        accumulator << element
        if accumulator.size == chunkSize
            yield accumulator
            accumulator = []
        end
    }
    if accumulator.size > 0
        yield accumulator
    end
end

class TestRunnerGnuParallel < TestRunner
    def prepareGnuParallelRunnerJobs(name, runlist, completedPlans)
        path = @runnerDir + name
        FileUtils.mkdir_p(@runnerDir)

        vs = (["-v"] * $verbosity).join(" ")
        ret = if $reportExecutionTime then " -r" else "" end
        pm = if $progressMeter then " -p" else "" end
        flags = "#{vs}#{ret}#{pm}"

        File.open(path, "w") { |outp|
            runlist = runlist.select { |plan|
                not completedPlans.include?(plan)
            }
            each_chunk(runlist, $gnuParallelChunkSize) { |plans|
                job = plans.collect { |plan|
                    "#{dispatcherCommand} ./testcase_#{plan.index} test #{flags} #{$runUniqueId}"
                }.join("; ")
                outp.puts(job)
            }
        }
    end
    def prepareRunner(runlist, serialPlans, completedPlans, remoteHosts)
        prepareGnuParallelRunnerJobs("parallel-tests", runlist, completedPlans | serialPlans)
        prepareGnuParallelRunnerJobs("serial-tests", serialPlans, completedPlans)
    end
end

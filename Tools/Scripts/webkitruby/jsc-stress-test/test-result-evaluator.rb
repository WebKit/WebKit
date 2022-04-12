# Copyright (C) 2021 Igalia S.L.
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

# Visits the tests in the runlist and invokes a number of
# callbacks. Used via subclassing.
class TestResultEvaluator
    def initialize(runlist, statusMap)
        @runlist = runlist
        @statusMap = statusMap
    end
    def noResult(plan)
        # XXX: maybe focus on incomplete in reporting?
    end
    def passed(plan)
    end
    def failed(plan)
    end
    def wasFlaky(plan, successes, tries, successful)
    end
    def visiting(plan)
    end
    def validate
    end
    def visit!(opts={})
        @runlist.each {
            |plan|
            visiting(plan)
            if not @statusMap.has_key?(plan.index)
                noResult(plan)
                next
            end
            statuses = @statusMap[plan.index]
            testWasSuccessful = nil
            if plan.retryParameters.nil?
                # If any of the results we got was a pass, consider the
                # test successful. The assumption here is that, unless a
                # test is marked as flaky, failures are because of
                # infrastructure issues (which is why the test was run
                # multiple times in the first place).
                testWasSuccessful = statuses.include?(STATUS_FILE_PASS)
            else
                testWasSuccessful = plan.retryParameters.result(statuses)
                if testWasSuccessful.nil?
                    if opts[:treatIncompleteAsFailed]
                        # Maximum number of iterations reached without a
                        # conclusive result; draw the developer's attention.
                        testWasSuccessful = false
                    else
                        # Not completed yet
                        next
                    end
                end
            end
            if testWasSuccessful
                passed(plan)
            else
                failed(plan)
            end
            nresults = statuses.uniq.size
            if nresults < 1
                $stderr.puts("Unexpected number of results: #{nresults} for plan #{plan}")
                raise "Unexpected number of results"
            elsif nresults != 1 and not plan.retryParameters.nil?
                # The flakiness stats are about diagnosing code issues, so only
                # record flakiness when we've intentionally retried the test.
                # Otherwise, multiple results might have come in because of
                # transient infrastructure failures causing the test to be
                # rescheduled on another host too. But then any failures might
                # be because the machine was under load (causing the test to hit
                # a timeout) or OOM'd.
                wasFlaky(plan, statuses.count(STATUS_FILE_PASS), statuses.size, testWasSuccessful)
            end
        }
    end
end

# Simple bean counting class with no side-effects.
class TestResultEvaluatorSimple < TestResultEvaluator
    attr_reader :testsPassed, :testsFailed
    def initialize(runlist, statusMap)
        super(runlist, statusMap)
        @testsPassed = Set.new
        @testsFailed = Set.new
        @noResult = Set.new
        @all = Set.new
    end
    def visiting(plan)
        @all.add(plan)
    end
    def passed(plan)
        @testsPassed.add(plan)
    end
    def failed(plan)
        @testsFailed.add(plan)
    end
    def noResult(plan)
        @noResult.add(plan)
    end
    def completed
        @testsPassed | @testsFailed
    end
    def missing
        @noResult
    end
    def validate
        counted = @testsPassed.size + @testsFailed.size + @noResult.size
        if counted != @all.size
            $stderr.puts("Accounting mismatch: expected #{@all.size} test results " +
                         "but got #{counted} (#{@testsPassed.size}, #{@testsFailed.size}, #{@noResult.size})")
            raise "Internal error in #{File.basename($0)}"
        end
    end
end

class ResultsWriter
    def initialize
        @files = {}
    end
    def append(filename, s)
        if not @files.has_key?(filename)
            @files[filename] = File.open($outputDir + filename, "a")
        end
        @files[filename].puts(s)
    end
    def appendFailure(plan)
        append("failed", plan.name)
    end
    def appendPass(plan)
        append("passed", plan.name)
    end
    def appendNoResult(plan)
        append("noresult", plan.name)
    end
    def appendFlaky(plan, successes, tries, successful)
        successful = successful ? 1 : 0
        append("flaky", "#{plan.name} #{successes} #{tries} #{successful}")
    end
    def appendResult(plan, didPass)
        append("results", "#{plan.name}: #{didPass ? 'PASS' : 'FAIL'}")
    end
    def close
        @files.each_value { |f|
            f.close
        }
        @files.clear
    end
end

# Final accounting of results.
class TestResultEvaluatorFinal < TestResultEvaluatorSimple
    attr_reader :noresult, :familyMap
    def initialize(runlist, statusMap)
        super(runlist, statusMap)
        @noresult = 0
        @familyMap = {}
        @writer = ResultsWriter.new
    end
    def noResult(plan)
        super(plan)
        @writer.appendNoResult(plan)
        @noresult += 1
    end
    def passed(plan)
        super(plan)
        @writer.appendPass(plan)
        appendResult(plan, true)
        addToFamilyMap(plan, "PASS")
    end
    def failed(plan)
        super(plan)
        @writer.appendFailure(plan)
        appendResult(plan, false)
        addToFamilyMap(plan, "FAIL")
    end
    def wasFlaky(plan, successes, tries, successful)
        @writer.appendFlaky(plan, successes, tries, successful)
    end
    private
    def appendResult(plan, successful)
        @writer.appendResult(plan, successful)
    end
    def addToFamilyMap(plan, result)
        unless familyMap[plan.family]
            familyMap[plan.family] = []
        end
        familyMap[plan.family] << { :result => result, :plan => plan}
    end
end

def verifyCompletedTestsMatchExpected(statusMap, completed, expected)
    if completed != expected
        unexpectedlyCompleted = (completed - expected).collect { |plan|
            "##{plan.index}: #{statusMap[plan.index]}"
        }.join(" | ")
        $stderr.puts("Unexpectedly completed: #{unexpectedlyCompleted}")
        expectedButNotCompleted = (expected - completed).collect { |plan|
            "##{plan.index}: #{statusMap[plan.index]}"
        }.join(" | ")
        $stderr.puts("Expected but not completed: #{expectedButNotCompleted}")
        raise "mismatch"
    end
end

module TestResultEvaluatorSelfTests
    def TestResultEvaluatorSelfTests.selfTestTestResultEvaluator
        runlist = []
        statusMap = {}
        expectedPass = Set.new
        expectedFail = Set.new
        tc = Struct.new(:statuses, :expectedPass)
        [
            tc.new([STATUS_FILE_PASS], true),
            tc.new([STATUS_FILE_PASS], true),
            tc.new([STATUS_FILE_PASS, STATUS_FILE_PASS], true),
            tc.new([STATUS_FILE_PASS, STATUS_FILE_FAIL], true),
            tc.new([STATUS_FILE_FAIL], false),
            tc.new([STATUS_FILE_FAIL, STATUS_FILE_FAIL], false),
        ].each_with_index {
            |test, testIndex|
            plan = BasePlan.mock("f", "t#{testIndex}")
            runlist << plan
            statusMap[plan.index] = test.statuses
            if test.expectedPass
                expectedPass.add(plan)
            else
                expectedFail.add(plan)
            end
        }
        evaluator = TestResultEvaluatorSimple.new(runlist, statusMap)
        evaluator.visit!

        if expectedPass != evaluator.testsPassed
            $stderr.puts("expectedPass: #{expectedPass}")
            $stderr.puts("Passed: #{evaluator.testsPassed}")
            raise "expectedPass mismatch"
        end
        if expectedFail != evaluator.testsFailed
            $stderr.puts("expectedFail: #{expectedFail}")
            $stderr.puts("Failed: #{evaluator.testsFailed}")
            raise "expectedFail mismatch"
        end
    end

    def getCompletedTestsFromStatusMap(runlist, statusMap)
        evaluator = TestResultEvaluatorSimple.new(runlist, statusMap)
        evaluator.visit!
        evaluator.completed
    end

    def TestResultEvaluatorSelfTests.selfTestGetCompletedTestsFromStatusMap
        runlist = []
        expectCompleted = Set.new
        statusMap = {}
        tc = Struct.new(:statuses, :retryParameters, :shouldCountAsCompleted)
        [
            # No results => not completed
            tc.new(nil, nil, false),
            tc.new(nil, RetryParameters.new(0.6, 2), false),

            # Passed/failed and not flaky -> completed
            tc.new([STATUS_FILE_PASS], nil, true),
            tc.new([STATUS_FILE_FAIL], nil, true),

            # If it has multiple results and is not a flaky, that means it
            # was re-run because of infrastructure issues. Counts as
            # completed regardless of the actual results.
            tc.new([STATUS_FILE_PASS, STATUS_FILE_PASS], nil, true),
            tc.new([STATUS_FILE_PASS, STATUS_FILE_FAIL], nil, true),
            tc.new([STATUS_FILE_FAIL, STATUS_FILE_FAIL], nil, true),

            # Not completed: we have one run remaining and this could flip
            # the percentage.
            tc.new([STATUS_FILE_PASS], RetryParameters.new(0.6, 2), false),
            tc.new([STATUS_FILE_FAIL], RetryParameters.new(0.4, 2), false),

            # Even if the last one fails we're still gonna be above
            # (respectively below) the pass percentage.
            tc.new([STATUS_FILE_PASS, STATUS_FILE_PASS], RetryParameters.new(0.5, 3), true),
            tc.new([STATUS_FILE_FAIL, STATUS_FILE_FAIL], RetryParameters.new(0.5, 3), true),
        ].each_with_index {
            |test, testIndex|
            plan = BasePlan.mock("f", "t#{testIndex}", test.retryParameters)
            runlist << plan
            if not test.statuses.nil?
                statusMap[plan.index] = test.statuses
            end
            if test.shouldCountAsCompleted
                expectCompleted.add(plan)
            end
        }
        evaluator = TestResultEvaluatorSimple.new(runlist, statusMap)
        evaluator.visit!

        verifyCompletedTestsMatchExpected(statusMap, evaluator.completed, expectCompleted)
    end
    def TestResultEvaluatorSelfTests.run
        TestResultEvaluatorSelfTests.selfTestTestResultEvaluator
        TestResultEvaluatorSelfTests.selfTestGetCompletedTestsFromStatusMap
    end
end

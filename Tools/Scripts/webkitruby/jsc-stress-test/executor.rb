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

# Implements the main loop for executing the tests and handling
# missing/multiple results (because of infrastructure issues). It also
# takes care of marking failing tests as flaky when
# --treat-failing-as-flaky is in effect. It's a class so that we can
# easily instantiate a subclass for testing.
class BaseTestsExecutor
    def initialize(runlist, serialPlans, remoteHosts, iterationLimits,
                   treatFailingAsFlaky, logStream=$stderr)
        @runlist = runlist
        @serialPlans = serialPlans
        @remoteHosts = remoteHosts
        @treatFailingAsFlaky = treatFailingAsFlaky
        @infraIterationsFloor = iterationLimits.infraIterationsFloor
        @iterationsCeiling = iterationLimits.iterationsCeiling
        if @infraIterationsFloor != 1 && @iterationsCeiling !=1 && @iterationsCeiling < @infraIterationsFloor
            raise "iterationsCeiling (#{@iterationsCeiling} < @infraIterationsFloor (#{@infraIterationsFloor}"
        end
        @maxIterations = setMaxIterations(initialMaxAllowedIterations(runlist))
        @logStream = logStream
    end
    def log(s)
        @logStream.puts(s) unless @logStream.nil?
    end
    def setMaxIterations(value)
        if value > @iterationsCeiling
            value = @iterationsCeiling
        end
        @maxIterations = value
    end
    def iterationsForInfraFailures
        # If we're running remotely, scale the extra iterations to
        # account for infrastructure failures by the number of hosts.
        numRemoteHosts = 0
        if not @remoteHosts.nil?
            numRemoteHosts = @remoteHosts.size / 2
        end
        [numRemoteHosts, @infraIterationsFloor].max
    end
    def initialMaxAllowedIterations(runlist)
        maxTries = 0
        runlist.each { |plan|
            next if plan.retryParameters.nil?
            if plan.retryParameters.maxTries > maxTries
                maxTries = plan.retryParameters.maxTries
            end
        }
        # The number of allowed iterations is the sum of
        # iterationsForInfraFailures and the max times we run a test
        # that appears to be flaky.
        iterationsForInfraFailures + maxTries
    end
    def adjustFlakinessForFailingTests(iteration, statusMap, evaluator)
        completedTests = evaluator.completed
        if @treatFailingAsFlaky.nil?
            return completedTests
        end

        # If we have a lot of failing tests, it's likely that the tree
        # is bad and there's no point in treating the failures as
        # potential flaky tests.
        if evaluator.testsFailed.size > @treatFailingAsFlaky.maxFailing
            return completedTests
        end
        adjustedTests = Set.new
        evaluator.testsFailed.each { |plan|
            if plan.retryParameters.nil?
                plan.retryParameters = RetryParameters.new(@treatFailingAsFlaky.passPercentage,
                                                           @treatFailingAsFlaky.maxTries)
                adjustedTests.add(plan)
            end
        }
        if adjustedTests.size > 0
            # Adjust the maximum number of tries upwards if we just
            # marked a failing test as flaky. Note that infrastructure
            # issues might cause test failures to be reported at later
            # iterations, so we use (+) instead of (max). This is safe
            # as the number of iterations is capped by
            # iterationsCeiling.
            newMaxIterations = @maxIterations + @treatFailingAsFlaky.maxTries
            if @maxIterations < newMaxIterations
                setMaxIterations(newMaxIterations)
            end
            putd("adjustFlakinessForFailingTests #{newMaxIterations} (->#{@maxIterations}) #{adjustedTests.collect { |p| p.to_s }}")
        end
        # We just marked those as flaky in order to rerun then, so
        # take them off the completedTests.
        ret = completedTests - adjustedTests
        putd("completedTests: #{ret.collect { |p| p.to_s }}")
        ret
    end
    def loop
        prepareArtifacts(@remoteHosts)
        statusMap = {}
        # This is a high-level retry loop. If a remote host goes away in
        # the middle of a run and either doesn't come back online or comes
        # back having removed the remote directory (*), getStatusMap will be
        # missing results for these tests, allowing us to retry them in
        # the next iteration.
        #
        # (*) This may well happen when running the tests on embedded
        # boards that regularly corrupt their SD card. Definitely an issue
        # for the ci20 boards used to test MIPS.
        100000.times { |iteration|
            if iteration >= @maxIterations
                break
            end
            # @remoteHosts (and therefore remoteHosts) will be nil
            # when not executing on remotes.
            remoteHosts = prepareExecution(@remoteHosts)
            if remoteHosts and remoteHosts.size == 0
                # Either everything is down, or everything got
                # rebooted. In the latter case, allow enough time for
                # the remote boards to boot up, then retry.
                waitInterval = 60
                log("All remote hosts failed, retrying after #{waitInterval}s")
                sleep(waitInterval)
                next
            end
            executeTests(remoteHosts)
            statusMap = updateStatusMap(iteration, statusMap)
            evaluator = TestResultEvaluatorSimple.new(@runlist, statusMap)
            evaluator.visit!

            completedTests = adjustFlakinessForFailingTests(iteration, statusMap, evaluator)
            # NB: evaluator.completed may now be out of date, we need
            # to use completedTests instead.

            break unless completedTests.size != @runlist.size
            if completedTests.size > @runlist.size
                raise "Test count mismatch: #{completedTests.size} > #{@runlist.size}"
            end

            # Regenerate the lists of tests to run
            refreshExecution(@runlist, @serialPlans, completedTests, @remoteHosts)

            numFlaky = @runlist.size - completedTests.size - evaluator.missing.size
            testsInfo = "completed #{completedTests.size}/#{@runlist.size} (#{numFlaky} flaky, #{evaluator.missing.size} missing)"
            remoteHostsInfo = ""
            if remoteHosts
                remoteHostsInfo = ", #{remoteHosts.size}/#{@remoteHosts.size} hosts live"
            end
            log("After try #{iteration + 1}/#{@maxIterations}: #{testsInfo}#{remoteHostsInfo}")
        }
        statusMap
    end
end

module ExecutorSelfTests
    class TestCase
        attr_reader :testsWithFlaky, :iterations, :expectedTestResults, :opts
        def initialize(testsWithFlaky, iterations, expectedTestResults, opts={})
            @testsWithFlaky = testsWithFlaky
            @iterations = iterations
            @expectedTestResults = expectedTestResults
            @opts = opts
        end
    end
    FinalTestResult = Struct.new(:result)
    TestResult = Struct.new(:results)
    Iteration = Struct.new(:testResults)
    MOCK_TEST_FAMILY = "f"
    ITERATION_LIMITS = OpenStruct.new(:infraIterationsFloor => 3,
                                      :iterationsCeiling => 8)
    class MockTestsExecutor < BaseTestsExecutor
        attr_reader :executedIterations
        def initialize(runlist, iterationResults, maxIterationsBounds, treatFailingAsFlaky, logStream)
            super(runlist, [], nil, maxIterationsBounds, treatFailingAsFlaky, logStream)
            @iterationResults = iterationResults
            @executedIterations = 0
        end
        def prepareArtifacts(remoteHosts)
        end
        def prepareExecution(remoteHosts)
            remoteHosts
        end
        def executeTests(remoteHosts)
        end
        def updateStatusMap(iteration, statusMap)
            if iteration >= @iterationResults.size
                $stderr.puts("Trying to execute iteration #{iteration} but only have #{@iterationResults.size} iteration results available")
                raise "Self test error: insufficient iterations"
            end
            @executedIterations += 1
            @iterationResults[iteration].each { |iteration|
                iteration.each_with_index { |testResult, testIndex|
                    index = @runlist[testIndex].index
                    # Because of infra issues, a test might have multiple results.
                    testResult.results.each { |result|
                        line = "#{index} #{$runUniqueId} 0 #{result}"
                        processStatusLine(statusMap, line)
                    }
                }
            }
            putd("statusMap: #{statusMap}")
            statusMap
        end
        def refreshExecution(runlist, serialPlans, completedTests, remoteHosts)
        end
    end

    def ExecutorSelfTests.run
        trf = TestResult.new([STATUS_FILE_FAIL])
        trp = TestResult.new([STATUS_FILE_PASS])
        tre = TestResult.new([])
        ftrp = FinalTestResult.new(STATUS_FILE_PASS)
        ftrf = FinalTestResult.new(STATUS_FILE_FAIL)
        ftre = FinalTestResult.new(nil)
        [
            TestCase.new([nil, nil], # 2 tests (t0, t1), neither flaky
                         [
                             Iteration.new(
                                 [
                                     trp, # t0 passed
                                     trp, # t1 passed
                                 ])
                         ],
                         [ftrp, ftrp]),
            TestCase.new([nil, nil], # 2 tests (t0, t1), neither flaky
                         [
                             Iteration.new([trp, trf]),
                         ],
                         [ftrp, ftrf]),
            # 1 test gives no result, need a second iteration, both finally pass
            TestCase.new([nil, nil],
                         [
                             Iteration.new([trp, tre]),
                             Iteration.new([tre, trp]),
                         ],
                         [ftrp, ftrp]),
            # 1 test fails, 1 gives no result, need a second iteration, one finally passes
            TestCase.new([nil, nil],
                         [
                             Iteration.new([trf, tre]),
                             Iteration.new([tre, trp]),
                         ],
                         [ftrf, ftrp]),
            # 1 test passes, 1 gives no result, need a second iteration, one finally passes
            TestCase.new([nil, nil],
                         [
                             Iteration.new([trp, tre]),
                             Iteration.new([tre, trf]),
                         ],
                         [ftrp, ftrf]),
            TestCase.new([nil, nil],
                         [
                             # First iteration doesn't produce any results.
                             Iteration.new([tre, tre]),
                             Iteration.new([trf, tre]),
                             Iteration.new([tre, trf]),
                         ],
                         [ftrf, ftrf]),
            # Test that, when no flakies are present, we stop at
            # infraIterationsFloor when no results come in.
            TestCase.new([nil, nil],
                         [Iteration.new([tre, tre])] * ITERATION_LIMITS.infraIterationsFloor,
                         [ftre, ftre]),
            # Test the maxIterations adjustment works as expected in
            # the presence of a static flaky.
            TestCase.new([RetryParameters.new(0.39, 4), nil],
                         [
                             # Since we have a flaky test, the maxIterations gets adjusted to
                             # ITERATION_LIMITS.infraIterationsFloor  + RetryParameters.maxTries = 7.
                             Iteration.new([tre, trp]),
                             Iteration.new([trp, tre]),
                             Iteration.new([trf, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([trp, tre]),
                         ],
                         [ftrp, ftrp]),
            # Test that a flaky can finish early.
            TestCase.new([RetryParameters.new(0.4, 4), nil],
                         [
                             Iteration.new([tre, trp]),
                             Iteration.new([trp, tre]),
                             Iteration.new([trp, tre]),
                         ],
                         [ftrp, ftrp]),
            # Same, with more variability.
            TestCase.new([nil, nil],
                         [
                             Iteration.new([trp, tre]),
                             Iteration.new([tre, trf]),
                             Iteration.new([tre, trp]),
                             Iteration.new([tre, trf]),
                             Iteration.new([tre, trp]),
                             Iteration.new([tre, trp]),
                         ],
                         [ftrp, ftrp],
                         {
                             :treatFailingAsFlaky => OpenStruct.new(:passPercentage => 0.4,
                                                                  :maxTries => 6,
                                                                  :maxFailing => 3)
                         }),
            # Same, with different variability.
            TestCase.new([nil, nil],
                         [
                             Iteration.new([trp, tre]),
                             Iteration.new([tre, trf]),
                             Iteration.new([tre, trp]),
                             Iteration.new([tre, trf]),
                             Iteration.new([tre, trp]),
                             Iteration.new([tre, trf]),
                             Iteration.new([tre, trf]),
                         ],
                         [ftrp, ftrf],
                         {
                             :treatFailingAsFlaky => OpenStruct.new(:passPercentage => 0.4,
                                                                    :maxTries => 6,
                                                                    :maxFailing => 3)
                         }),
            # Test that a flaky for which not enough results come in is not marked as completed.
            TestCase.new([nil],
                         [
                             Iteration.new([trf]),
                             Iteration.new([trp]),
                             Iteration.new([trf]),
                             Iteration.new([tre]),
                             Iteration.new([tre]),
                             Iteration.new([tre]),
                             Iteration.new([tre]),
                         ],
                         [ftre],
                         {
                             :treatFailingAsFlaky => OpenStruct.new(:passPercentage => 0.49,
                                                                    :maxTries => 4,
                                                                    :maxFailing => 1),
                             :acceptIncomplete => true,
                         }),
            # Test that (statically) flaky tests aren't adjusted
            TestCase.new([RetryParameters.new(0.5, 3), nil],
                         [
                             Iteration.new([trf, trp]),
                             Iteration.new([trp, tre]),
                             Iteration.new([trf, tre]),
                         ],
                         [ftrf, ftrp],
                         {
                             :treatFailingAsFlaky => OpenStruct.new(:passPercentage => 0.7, # > 2/3
                                                                    :maxTries => 6,
                                                                    :maxFailing => 3)
                         }),
            # Test that we respect maxFailing.
            TestCase.new([nil, nil, nil],
                         [
                             Iteration.new([trf, trf, trf]),
                         ],
                         [ftrf, ftrf, ftrf],
                         {
                             :treatFailingAsFlaky => OpenStruct.new(:passPercentage => 0.7, # > 2/3
                                                                    :maxTries => 6,
                                                                    :maxFailing => 2)
                         }),
            # Test that we halt at maxIterations (3 + 5 = 8) if no results come in.
            TestCase.new([RetryParameters.new(0.5, 5), nil],
                         [
                             Iteration.new([tre, trp]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                             Iteration.new([tre, tre]),
                         ],
                         [ftre, ftrp]),
        ].each_with_index {
            |testcase, testcaseIndex|
            runlist = []
            putd("===> Testcase #{testcaseIndex}")
            testcase.testsWithFlaky.each_with_index { |retryParameters, index|
                plan = BasePlan.mock(MOCK_TEST_FAMILY, "t#{index}", retryParameters)
                runlist << plan
            }
            putd("runlist: #{runlist.collect { |p| p.to_s }}, expected: #{testcase.expectedTestResults}")
            executor = MockTestsExecutor.new(runlist, testcase.iterations,
                                             ITERATION_LIMITS, testcase.opts[:treatFailingAsFlaky],
                                             $testsDebugStream)
            statusMap = executor.loop
            if executor.executedIterations != testcase.iterations.size
                # The testcase should provide exactly as many
                # iteration results as needed.
                raise "Executed #{executor.executedIterations}, #{testcase.iterations.size} given"
            end
            expectCompleted = Set.new(runlist.select { |plan|
                                          not testcase.expectedTestResults[runlist.index(plan)].result.nil?
                                      })

            evaluator = TestResultEvaluatorSimple.new(runlist, statusMap)
            evaluator.visit!
            if not testcase.opts[:acceptIncomplete]
                evaluator.validate
            end
            verifyCompletedTestsMatchExpected(statusMap, evaluator.completed, expectCompleted)

            testcaseFailed = false
            runlist.each_with_index { |plan, index|
                case testcase.expectedTestResults[index].result
                when nil
                    if evaluator.testsPassed.include?(plan)
                        testcaseFailed = true
                        $stderr.puts("Test #{index} should not have completed but passed")
                    elsif evaluator.testsFailed.include?(plan)
                        testcaseFailed = true
                        $stderr.puts("Test #{index} should not have completed but failed")
                    end
                when STATUS_FILE_PASS
                    if not evaluator.testsPassed.include?(plan)
                        testcaseFailed = true
                        $stderr.puts("Test #{index} should have passed but didn't")
                    end
                when STATUS_FILE_FAIL
                    if not evaluator.testsFailed.include?(plan)
                        testcaseFailed = true
                        $stderr.puts("Test #{index} should have failed but didn't")
                    end
                else
                    raise "Can't get here"
                end
            }
            if testcaseFailed
                $stderr.puts("statusMap = #{statusMap}")
                raise "Self test failure"
            end
        }
    end
end

# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

import argparse
import collections
import json
import math
import os
import re
import subprocess
import sys

jitTests = ["3d-cube-SP", "3d-raytrace-SP", "acorn-wtb", "ai-astar", "Air", "async-fs", "Babylon", "babylon-wtb", "base64-SP", "Basic", "Box2D", "cdjs", "chai-wtb", "coffeescript-wtb", "crypto", "crypto-aes-SP", "crypto-md5-SP", "crypto-sha1-SP", "date-format-tofte-SP", "date-format-xparb-SP", "delta-blue", "earley-boyer", "espree-wtb", "first-inspector-code-load", "FlightPlanner", "float-mm.c", "gaussian-blur", "gbemu", "gcc-loops-wasm", "hash-map", "HashSet-wasm", "jshint-wtb", "json-parse-inspector", "json-stringify-inspector", "lebab-wtb", "mandreel", "ML", "multi-inspector-code-load", "n-body-SP", "navier-stokes", "octane-code-load", "octane-zlib", "OfflineAssembler", "pdfjs", "prepack-wtb", "quicksort-wasm", "raytrace", "regex-dna-SP", "regexp", "richards", "richards-wasm", "splay", "stanford-crypto-aes", "stanford-crypto-pbkdf2", "stanford-crypto-sha256", "string-unpack-code-SP", "tagcloud-SP", "tsf-wasm", "typescript", "uglify-js-wtb", "UniPoker", "WSL"]

nonJITTests = ["3d-cube-SP", "3d-raytrace-SP", "acorn-wtb", "ai-astar", "Air", "async-fs", "Babylon", "babylon-wtb", "base64-SP", "Basic", "Box2D", "cdjs", "chai-wtb", "coffeescript-wtb", "crypto-aes-SP", "delta-blue", "earley-boyer", "espree-wtb", "first-inspector-code-load", "gaussian-blur", "gbemu", "hash-map", "jshint-wtb", "json-parse-inspector", "json-stringify-inspector", "lebab-wtb", "mandreel", "ML", "multi-inspector-code-load", "octane-code-load", "OfflineAssembler", "pdfjs", "prepack-wtb", "raytrace", "regex-dna-SP", "regexp", "splay", "stanford-crypto-aes", "string-unpack-code-SP", "tagcloud-SP", "typescript", "uglify-js-wtb"]

# Run two groups of tests with each group in a single JSC instance to see how well memory recovers between tests.
groupTests = ["typescript,acorn-wtb,Air,pdfjs,crypto-aes-SP", "splay,FlightPlanner,prepack-wtb,octane-zlib,3d-cube-SP"]

luaTests = [("hello_world-LJF", "LuaJSFight/hello_world.js", 5), ("list_search-LJF", "LuaJSFight/list_search.js", 5), ("lists-LJF", "LuaJSFight/lists.js", 5), ("string_lists-LJF", "LuaJSFight/string_lists.js", 5), ("richards", "LuaJSFight/richards.js", 5)]

oneMB = float(1024 * 1024)
footprintRE = re.compile(r"Current Footprint: (\d+(?:.\d+)?)")
peakFootprintRE = re.compile(r"Peak Footprint: (\d+(?:.\d+)?)")

TestResult = collections.namedtuple("TestResult", ["name", "returnCode", "footprint", "peakFootprint", "vmmapOutput", "smapsOutput"])

ramification_dir = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))

remoteHooks = {}


def mean(values):
    if not len(values):
        return None

    return sum(values) / len(values)


def geomean(values):
    if not len(values):
        return None

    product = 1.0

    for x in values:
        product *= x

    return math.pow(product, (1.0 / len(values)))


def frameworkPathFromExecutablePath(execPath):
    if not os.path.abspath(execPath):
        execPath = os.path.isabs(execPath)

    pathMatch = re.match("(.*?/WebKitBuild/(Release|Debug)+)/([a-zA-Z]+)$", execPath)
    if pathMatch:
        return pathMatch.group(1)

    pathMatch = re.match("(.*?)/JavaScriptCore.framework/Resources/([a-zA-Z]+)$", execPath)
    if pathMatch:
        return pathMatch.group(1)

    pathMatch = re.match("(.*?)/JavaScriptCore.framework/Helpers/([a-zA-Z]+)$", execPath)
    if pathMatch:
        return pathMatch.group(1)

    pathMatch = re.match("(.*?/(Release|Debug)+)/([a-zA-Z]+)$", execPath)
    if pathMatch:
        return pathMatch.group(1)

    pathMatch = re.match("(.*?)/JavaScriptCore.framework/(.*?)/Resources/([a-zA-Z]+)$", execPath)
    if pathMatch:
        return pathMatch.group(1)


def parseArgs(parser=None):
    def optStrToBool(arg):
        if arg.lower() in ("true", "t", "yes", "y"):
            return True
        if arg.lower() in ("false", "f", "no", "n"):
            return False

        raise argparse.ArgumentTypeError("Boolean value expected")

    if not parser:
        parser = argparse.ArgumentParser(description="RAMification benchmark driver script")
        parser.set_defaults(runner=LocalRunner)

    verbosityGroup = parser.add_mutually_exclusive_group()
    verbosityGroup.add_argument("-q", "--quiet", dest="verbose", action="store_false", help="Provide less output")
    verbosityGroup.add_argument("-v", "--verbose", dest="verbose", action="store_true", default=True, help="Provide more output")

    parser.add_argument("-c", "--jsc", dest="jscCommand", type=str, default="/usr/local/bin/jsc", metavar="path-to-jsc", help="Path to jsc command")
    parser.add_argument("-d", "--jetstream3-dir", dest="testDir", type=str, default=ramification_dir, metavar="path-to-JetStream3-files", help="JetStream3 root directory")
    parser.add_argument("-e", "--env-var", dest="extraEnvVars", action="append", default=[], metavar="env-var=value", help="Specify additional environment variables")
    parser.add_argument("-f", "--format-json", dest="formatJSON", action="store_true", default=False, help="Format JSON with whitespace")
    parser.add_argument("-g", "--run-grouped-tests", dest="runGroupedTests", nargs="?", const=True, default=None, type=optStrToBool, metavar="true / false", help="Run grouped tests [default]")
    parser.add_argument("-j", "--run-jit", dest="runJITTests", nargs="?", const=True, default=None, type=optStrToBool, metavar="true / false", help="Run JIT tests [default]")
    parser.add_argument("-l", "--lua", dest="runLuaTests", nargs="?", const=True, default=None, type=optStrToBool, metavar="true / false", help="Run Lua comparison tests [default]")
    parser.add_argument("-n", "--run-no-jit", dest="runNoJITTests", nargs="?", const=True, default=None, type=optStrToBool, metavar="true / false", help="Run no JIT tests [default]")
    parser.add_argument("-o", "--output", dest="jsonFilename", type=str, default=None, metavar="JSON-output-file", help="Path to JSON output")
    parser.add_argument("-m", "--vmmap", dest="takeVmmap", action="store_true", default=False, help="Take a vmmap after each test")
    parser.add_argument("--smaps", dest="takeSmaps", action="store_true", default=False, help="Take a smaps rollup after each test")

    args = parser.parse_args()

    subtestArgs = [args.runGroupedTests, args.runJITTests, args.runLuaTests, args.runNoJITTests]
    allDefault = all([arg is None for arg in subtestArgs])
    anyTrue = any([arg is True for arg in subtestArgs])
    anyFalse = any([arg is False for arg in subtestArgs])

    # Default behavior is to run all subtests.
    # If a test is explicitly specified not to run, skip that test and use the default behavior for the remaining tests.
    # If tests are explicitly specified to run, only run those tests.
    # If there is a mix of tests specified to run and not to run, also do not run any unspecified tests.
    getArgValue = lambda arg: True if allDefault else False if arg is None and anyTrue else True if arg is None and anyFalse else arg

    args.runJITTests = getArgValue(args.runJITTests)
    args.runNoJITTests = getArgValue(args.runNoJITTests)
    args.runLuaTests = getArgValue(args.runLuaTests)
    args.runGroupedTests = getArgValue(args.runGroupedTests)

    return args


class BaseRunner:
    def __init__(self, args):
        self.rootDir = args.testDir
        self.environmentVars = {}
        self.vmmapOutput = "" if args.takeVmmap else None
        self.smapsOutput = "" if args.takeSmaps else None

    def setup(self):
        pass

    def setEnv(self, variable, value):
        self.environmentVars[variable] = value

    def unsetEnv(self, variable):
        self.environmentVars.pop(variable, None)

    def resetForTest(self, testName):
        self.testName = testName
        self.footprint = None
        self.peakFootprint = None
        self.returnCode = 0

    def processLine(self, line):
        line = str(line.strip())

        footprintMatch = re.match(footprintRE, line)
        if footprintMatch:
            self.footprint = int(footprintMatch.group(1))
            return

        peakFootprintMatch = re.match(peakFootprintRE, line)
        if peakFootprintMatch:
            self.peakFootprint = int(peakFootprintMatch.group(1))

    def setReturnCode(self, returnCode):
        self.returnCode = returnCode

    def getResults(self):
        return TestResult(name=self.testName, returnCode=self.returnCode, footprint=self.footprint, peakFootprint=self.peakFootprint, vmmapOutput=self.vmmapOutput, smapsOutput=self.smapsOutput)


class LocalRunner(BaseRunner):
    def __init__(self, args):
        BaseRunner.__init__(self, args)
        self.jscCommand = args.jscCommand

    def runOneTest(self, test, extraOptions=None, useJetStream3Harness=True):
        self.resetForTest(test)

        args = [self.jscCommand]
        if extraOptions:
            args.extend(extraOptions)

        if useJetStream3Harness:
            args.extend(["-e", "testList='{test}'; runMode='RAMification'".format(test=test), "cli.js"])
        else:
            args.extend(["--footprint", "{test}".format(test=test)])

        self.resetForTest(test)

        proc = subprocess.Popen(args, cwd=self.rootDir, env=self.environmentVars, stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=None, shell=False)
        while True:
            line = proc.stdout.readline()
            if sys.version_info[0] >= 3:
                line = str(line, "utf-8")

            self.processLine(line)

            if "js shell waiting for input to exit" in line:
                if self.vmmapOutput is not None:
                    self.vmmapOutput = subprocess.Popen(['vmmap', '--summary', '{}'.format(proc.pid)], shell=False, stderr=subprocess.PIPE, stdout=subprocess.PIPE).stdout.read()
                    if sys.version_info[0] >= 3:
                        self.vmmapOutput = str(self.vmmapOutput, "utf-8")
                if self.smapsOutput is not None:
                    self.smapsOutput = subprocess.Popen(['cat', '/proc/{}/smaps_rollup'.format(proc.pid)], shell=False, stderr=subprocess.PIPE, stdout=subprocess.PIPE).stdout.read()
                proc.stdin.write(b"done\n")
                proc.stdin.flush()

            if line == "":
                break

        self.setReturnCode(proc.wait())

        return self.getResults()


def main(parser=None):
    footprintValues = []
    peakFootprintValues = []
    testResultsDict = {}
    hasFailedRuns = False

    args = parseArgs(parser=parser)

    testRunner = args.runner(args)

    if args.takeVmmap or args.takeSmaps:
        testRunner.setEnv("JS_SHELL_WAIT_FOR_INPUT_TO_EXIT", "1")

    dyldFrameworkPath = frameworkPathFromExecutablePath(args.jscCommand)
    if dyldFrameworkPath:
        testRunner.setEnv("DYLD_FRAMEWORK_PATH", dyldFrameworkPath)

    for envVar in args.extraEnvVars:
        envVarParts = envVar.split("=")
        if len(envVarParts) == 1:
            envVarParts[1] = "1"
        testRunner.setEnv(envVarParts[0], envVarParts[1])

    testRunner.setup()

    def runTestList(testList, extraOptions=None, useJetStream3Harness=True):
        testScoresDict = {}

        for testInfo in testList:
            footprintScores = []
            peakFootprintScores = []
            if isinstance(testInfo, tuple):
                testName, test, weight = testInfo
            else:
                testName, test, weight = testInfo, testInfo, 1

            sys.stdout.write("Running {}... ".format(testName))
            testResult = testRunner.runOneTest(test, extraOptions, useJetStream3Harness)

            if testResult.returnCode == 0 and testResult.footprint and testResult.peakFootprint:
                if args.verbose:
                    print("footprint: {}, peak footprint: {}".format(testResult.footprint, testResult.peakFootprint))
                    if testResult.vmmapOutput:
                        print(testResult.vmmapOutput)
                    if testResult.smapsOutput:
                        print(testResult.smapsOutput)
                else:
                    print

                if testResult.footprint:
                    footprintScores.append(int(testResult.footprint))
                    for count in range(0, weight):
                        footprintValues.append(testResult.footprint / oneMB)
                if testResult.peakFootprint:
                    peakFootprintScores.append(int(testResult.peakFootprint))
                    for count in range(0, weight):
                        peakFootprintValues.append(testResult.peakFootprint / oneMB)
            else:
                hasFailedRuns = True
                print("failed")
                if testResult.returnCode != 0:
                    print(" exit code = {}".format(testResult.returnCode))
                if not testResult.footprint:
                    print(" footprint = {}".format(testResult.footprint))
                if not testResult.peakFootprint:
                    print(" peak footprint = {}".format(testResult.peakFootprint))
                print

            testScoresDict[test] = {"metrics": {"Allocations": ["Geometric"]}, "tests": {"end": {"metrics": {"Allocations": {"current": footprintScores}}}, "peak": {"metrics": {"Allocations": {"current": peakFootprintScores}}}}}

        return testScoresDict

    current_path = os.getcwd()
    os.chdir(ramification_dir)  # To allow JS libraries to load

    if args.runLuaTests:
        if args.verbose:
            print("== LuaJSFight No JIT tests ==")

        # Use system malloc for LuaJSFight tests
        testRunner.setEnv("Malloc", "X")

        scoresDict = runTestList(luaTests, ["--useJIT=false", "--forceMiniVMMode=true"], useJetStream3Harness=False)

        testResultsDict["LuaJSFight No JIT Tests"] = {"metrics": {"Allocations": ["Geometric"]}, "tests": scoresDict}

        testRunner.unsetEnv("Malloc")

    if args.runGroupedTests:
        if args.verbose:
            print("== Grouped tests ==")

        scoresDict = runTestList(groupTests)

        testResultsDict["Grouped Tests"] = {"metrics": {"Allocations": ["Geometric"]}, "tests": scoresDict}

    if args.runJITTests:
        if args.verbose:
            print("== JIT tests ==")

        scoresDict = runTestList(jitTests)

        testResultsDict["JIT Tests"] = {"metrics": {"Allocations": ["Geometric"]}, "tests": scoresDict}

    if args.runNoJITTests:
        if args.verbose:
            print("== No JIT tests ==")

        scoresDict = runTestList(nonJITTests, ["--useJIT=false", "-e", "testIterationCount=1"])

        testResultsDict["No JIT Tests"] = {"metrics": {"Allocations": ["Geometric"]}, "tests": scoresDict}

    footprintGeomean = int(geomean(footprintValues) * oneMB)
    peakFootprintGeomean = int(geomean(peakFootprintValues) * oneMB)
    totalScore = int(geomean([footprintGeomean, peakFootprintGeomean]))

    if footprintGeomean:
        print("Footprint geomean: {} ({:.3f} MB)".format(footprintGeomean, footprintGeomean / oneMB))

    if peakFootprintGeomean:
        print("Peak Footprint geomean: {} ({:.3f} MB)".format(peakFootprintGeomean, peakFootprintGeomean / oneMB))

    if footprintGeomean and peakFootprintGeomean:
        print("Score: {} ({:.3f} MB)".format(totalScore, totalScore / oneMB))

    resultsDict = {"RAMification": {"metrics": {"Allocations": {"current": [totalScore]}}, "tests": testResultsDict}}

    os.chdir(current_path)  # Reset the path back to what it was before

    if args.jsonFilename:
        with open(args.jsonFilename, "w") as jsonFile:
            if args.formatJSON:
                json.dump(resultsDict, jsonFile, indent=4, separators=(',', ': '))
            else:
                json.dump(resultsDict, jsonFile)

    if hasFailedRuns:
        print("Detected failed run(s), exiting with non-zero return code")

    return hasFailedRuns


if __name__ == "__main__":
    exit(main())

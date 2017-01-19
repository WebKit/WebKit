// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This script loads all the test/* files into a very small context that
// only exposes a minimal set of functions that allows to register tests.
//
// Once all files are loaded it runs the specific test on the command line.
// If no arguments are given it lists all the registered tests.
//
// Note: the small context where the scripts are loaded is intended to keep
// nodejs-isms away from the test code and isolate implementation details away
// from them.
var fs = require('fs');
var vm = require('vm');
var Test = require('./test.js');

var testSuites = {};

function registerTest(name, func) {
  testSuites[name] = func;
}

function registerBotTest(name, func, bots) {
  registerTest(name, bootstrap);

  function bootstrap(test) {
    var callbacks = [];
    for (var i = 0; i != bots.length; ++i)
      callbacks.push(test.spawnBot.bind(test, "", bots[i]));

    test.wait(callbacks, func.bind(test, test));
  }
}

function loadTestFile(filename, doneCallback) {
  var loadTestContext = {
    setTimeout: setTimeout,
    registerTest: registerTest,
    registerBotTest: registerBotTest
  };
  var script = vm.createScript(fs.readFileSync(filename), filename);
  script.runInNewContext(loadTestContext);
  doneCallback();
}

function iterateOverTestFiles(foreachCallback, doneCallback) {
  fs.readdir('test', function (error, list) {
    function iterateNextFile() {
      if (list.length === 0) {
        doneCallback();
      } else {
        var filename = list.pop();
        if (filename[0] === '.' || filename.slice(-3) !== '.js') {
          // Skip hidden and non .js files on that directory.
          iterateNextFile();
        } else {
          foreachCallback('test/' + filename, iterateNextFile);
        }
      }
    }

    if (error !== null) {
      throw error;
    }
    iterateNextFile();
  });
}

function runTest(testname) {
  if (testname in testSuites) {
    console.log("Running test: " + testname);
    var test = new Test();
    testSuites[testname](test);
  } else {
    console.log("Unknown test: " + testname);
  }
}

function printUsage() {
  console.log('Run as:\n $ '
      + process.argv[0] + ' ' + process.argv[1]
      + ' <testname>');
  console.log('These are the existent ones:');
  for (var testname in testSuites)
    console.log('  ' + testname);
}

function main() {
  // TODO(andresp): support multiple tests.
  var testList = process.argv.slice(2);
  if (testList.length === 1)
    runTest(testList[0]);
  else
    printUsage();
}

iterateOverTestFiles(loadTestFile, main);

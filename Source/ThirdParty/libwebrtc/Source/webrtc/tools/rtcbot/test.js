// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Provides a Test class that exposes api to the tests.
// Read test.prototype to see what methods are exposed.
var fs = require('fs');
var request = require('request');
var BotManager = require('./botmanager.js');

function Test() {
  this.timeout_ = setTimeout(
      this.fail.bind(this, "Test timeout!"),
      100000);
}

Test.prototype = {
  log: function () {
    console.log.apply(console.log, arguments);
  },

  abort: function (error) {
    var error = new Error(error || "Test aborted");
    console.log(error.stack);
    process.exit(1);
  },

  assert: function (value, message) {
    if (value !== true) {
      this.abort(message || "Assert failed.");
    }
  },

  fail: function () {
    this.assert(false, "Test failed.");
  },

  done: function () {
    clearTimeout(this.timeout_);
    console.log("Test succeeded");
    process.exit(0);
  },

  // Utility method to wait for multiple callbacks to be executed.
  //  functions - array of functions to call with a callback.
  //  doneCallback - called when all callbacks on the array have completed.
  wait: function (functions, doneCallback) {
    var result = new Array(functions.length);
    var missingResult = functions.length;
    for (var i = 0; i != functions.length; ++i)
      functions[i](complete.bind(this, i));

    function complete(index, value) {
      missingResult--;
      result[index] = value;
      if (missingResult == 0)
        doneCallback.apply(null, result);
    }
  },

  spawnBot: function (name, botType, doneCallback) {
    // Lazy initialization of botmanager.
    if (!this.botManager_)
      this.botManager_ = new BotManager();
    this.botManager_.spawnNewBot(name, botType, doneCallback);
  },

  createStatisticsReport: function (outputFileName) {
    return new StatisticsReport(outputFileName);
  },

  // Ask computeengineondemand to give us TURN server credentials and URIs.
  createTurnConfig: function (onSuccess, onError) {
    request('https://computeengineondemand.appspot.com/turn?username=1234&key=5678',
        function (error, response, body) {
          if (error || response.statusCode != 200) {
            onError('TURN request failed');
            return;
          }

          var response = JSON.parse(body);
          var iceServer = {
            'username': response.username,
            'credential': response.password,
            'urls': response.uris
          };
          onSuccess({ 'iceServers': [ iceServer ] });
        }
      );
  },
}

StatisticsReport = function (outputFileName) {
  this.output_ = [];
  this.output_.push("Version: 1");
  this.outputFileName_ = outputFileName;
}

StatisticsReport.prototype = {
  collectStatsFromPeerConnection: function (prefix, pc) {
    setInterval(this.addPeerConnectionStats.bind(this, prefix, pc), 100);
  },

  addPeerConnectionStats: function (prefix, pc) {
    pc.getStats(onStatsReady.bind(this));

    function onStatsReady(reports) {
      for (index in reports) {
        var stats = {};
        stats[reports[index].id] = collectStats(reports[index].stats);

        var data = {};
        data[prefix] = stats;

        this.output_.push({
            type: "UpdateCounters",
            startTime: (new Date()).getTime(),
            data: data,
          });
      }
    };

    function collectStats(stats) {
      var outputStats = {};
      for (index in stats) {
        var statValue = parseFloat(stats[index].stat);
        outputStats[stats[index].name] = isNaN(statValue)?
            stats[index].stat : statValue;
      }
      return outputStats;
    };
  },

  finish: function (doneCallback) {
    fs.exists("test/reports/", function (exists) {
      if(exists) {
        writeFile.bind(this)();
      } else {
        fs.mkdir("test/reports/", 0777, writeFile.bind(this));
      }
    }.bind(this));

    function writeFile () {
      fs.writeFile("test/reports/" + this.outputFileName_ + "_" +
        (new Date()).getTime() +".json", JSON.stringify(this.output_),
        doneCallback);
    }
  },
};

module.exports = Test;

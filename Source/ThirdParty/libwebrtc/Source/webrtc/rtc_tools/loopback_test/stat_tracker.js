/**
 * Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

// StatTracker is a helper class to keep track of stats on a RTCPeerConnection
// object. It uses google visualization datatables to keep the recorded samples
// and simplify plugging them into graphs later.
//
// Usage example:
//   var tracker = new StatTracker(pc, pollInterval);
//   tracker.recordStat("EstimatedSendBitrate",
//                      "bweforvideo", "googAvailableSendBandwidth");
//   ...
//   tracker.stop();
//   tracker.dataTable(); // returns the recorded values. In this case
//   a table with 2 columns { Time, EstimatedSendBitrate } and a row for each
//   sample taken until stop() was called.
//
function StatTracker(pc, pollInterval) {
  pollInterval = pollInterval || 250;

  var dataTable = new google.visualization.DataTable();
  var timeColumnIndex = dataTable.addColumn('datetime', 'Time');
  var recording = true;

  // Set of sampling functions. Functions registered here are called
  // once per getStats with the given report and a rowIndex for the
  // sample period so they can extract and record the tracked variables.
  var samplingFunctions = {};

  // Accessor to the current recorded stats.
  this.dataTable = function() { return dataTable; }

  // recordStat(varName, recordName, statName) adds a samplingFunction that
  // records namedItem(recordName).stat(statName) from RTCStatsReport for each
  // sample into a column named varName in the dataTable.
  this.recordStat = function (varName, recordName, statName) {
    var columnIndex = dataTable.addColumn('number', varName);
    samplingFunctions[varName] = function (report, rowIndex) {
      var sample;
      var record = report.namedItem(recordName);
      if (record) sample = record.stat(statName);
      dataTable.setCell(rowIndex, columnIndex, sample);
    }
  }

  // Stops the polling of stats from the peer connection.
  this.stop = function() {
    recording = false;
  }

  // RTCPeerConnection.getStats is asynchronous. In order to avoid having
  // too many pending getStats requests going, this code only queues the
  // next getStats with setTimeout after the previous one returns, instead
  // of using setInterval.
  function poll() {
    pc.getStats(function (report) {
      if (!recording) return;
      setTimeout(poll, pollInterval);
      var result = report.result();
      if (result.length < 1) return;

      var rowIndex = dataTable.addRow();
      dataTable.setCell(rowIndex, timeColumnIndex, result[0].timestamp);
      for (var v in samplingFunctions)
        samplingFunctions[v](report, rowIndex);
    });
  }
  setTimeout(poll, pollInterval);
}

/**
 * Utility method to perform a full join between data tables from StatTracker.
 */
function mergeDataTable(dataTable1, dataTable2) {
  function allColumns(cols) {
    var a = [];
    for (var i = 1; i < cols; ++i) a.push(i);
    return a;
  }
  return google.visualization.data.join(
      dataTable1,
      dataTable2,
      'full',
      [[0, 0]],
      allColumns(dataTable1.getNumberOfColumns()),
      allColumns(dataTable2.getNumberOfColumns()));
}

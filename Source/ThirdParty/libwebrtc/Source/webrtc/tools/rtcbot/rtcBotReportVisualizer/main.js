// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
google.load("visualization", "1", {packages:["corechart"]});

function openFiles(event) {
  var files = event.target.files;
  readAndAnalyzeFiles(files)
}

function readAndAnalyzeFiles(files) {
  if(!files) {
    alert("No files have been selected!");
    return;
  }

  var reports = [];
  var filesNames = [];
  missingFiles = files.length;

  for(var i = 0; i < files.length; i++) {
    var reader = new FileReader();
    reader.onload = onReaderLoad.bind(reader, files[i].name);
    reader.readAsText(files[i]);
  }

  function onReaderLoad(fileName) {
    reports.push(JSON.parse(this.result));
    filesNames.push(fileName);

    missingFiles--;
    if(missingFiles == 0) {
      analyzeReports_(reports, filesNames);
    }
  }
}

// TODO(houssainy) take the input stats from the select list or
// drop down menu in html.
function analyzeReports_(reports, filesNames) {
  filesNames.unshift(""); // ned

  // Rtt
  analyzeRttData(reports, filesNames, "bot1");
  analyzeRttData(reports, filesNames, "bot2");

  // Send Packets Lost
  analyzePacketsLostData(reports, filesNames, "bot1");
  analyzePacketsLostData(reports, filesNames, "bot2");

  // Send bandwidth
  analyzeData(reports, filesNames, "Available Send Bandwidth-bot1", "bot1",
      "bweforvideo", "googAvailableSendBandwidth");
  analyzeData(reports, filesNames, "Available Send Bandwidth-bot2", "bot2",
      "bweforvideo", "googAvailableSendBandwidth");

   // Receive bandwidth
   analyzeData(reports, filesNames, "Available Receive Bandwidth-bot1", "bot1",
       "bweforvideo", "googAvailableReceiveBandwidth");
   analyzeData(reports, filesNames, "Available Receive Bandwidth-bot2", "bot2",
     "bweforvideo", "googAvailableReceiveBandwidth");

  drawSeparatorLine();
}

function analyzeRttData(reports, filesNames, botName) {
  var outPut = [];
  outPut.push(filesNames);

  var avergaData = ['Average Rtt x10'];
  var maxData = ['Max Rtt'];

  var average;
  var max;
  for(var index in reports) {
    average = getStateAverage(reports[index], botName, "Conn-audio-1-0",
      "googRtt");
    avergaData.push(average*10);

    max = getStateMax(reports[index], botName, "Conn-audio-1-0",
      "googRtt");
    maxData.push(max);
  }
  outPut.push(avergaData);
  outPut.push(maxData);

  drawChart("Rtt-" + botName, outPut);
}

function analyzePacketsLostData(reports, filesNames, botName) {
  var outPut = [];
  outPut.push(filesNames);

  var maxData = ['Max Send PacketsLost'];
  var max;
  for(var index in reports) {
    max = getStateMax(reports[index], botName, "ssrc_[0-9]+_send",
        "packetsLost");
    maxData.push(max);
  }
  outPut.push(maxData);

  drawChart("Send PacketsLost-" + botName, outPut);
}

function analyzeData(reports, filesNames, chartName, botName, reportId,
    statName) {
  var outPut = [];
  outPut.push(filesNames);

  var avergaData = ['Average ' + statName];
  var maxData = ['Max ' + statName];

  var average;
  var max;
  for(var index in reports) {
    average = getStateAverage(reports[index], botName, reportId, statName);
    avergaData.push(average);

    max = getStateMax(reports[index], botName, reportId, statName);
    maxData.push(max);
  }
  outPut.push(avergaData);
  outPut.push(maxData);

  drawChart(chartName, outPut);
}

function getStateAverage(reports, botName, reportId, statName) {
  var sum = 0;
  var count = 0;

  for (var index in reports) {
    var data = reports[index].data;
    if(index == 0 || !data.hasOwnProperty(botName))
      continue;

    var stats = data[botName];
    for (var key in stats) {
      if(key.search(reportId) != -1) {
        var value = parseInt(stats[key][statName]);
        sum += value;
        count++;
      }
    }
  }
  return Math.round(sum/count);
}

function getStateMax(reports, botName, reportId, statName) {
  var max = -1;

  for (var index in reports) {
    var data = reports[index].data;
    if(index == 0 || !data.hasOwnProperty(botName))
      continue;

    var stats = data[botName];
    for (var key in stats) {
      if(key.search(reportId) != -1) {
        var value = parseInt(stats[key][statName]);
        max = Math.max(value, max);
      }
    }
  }
  return max;
}

function drawChart(title, data) {
  var dataTable = google.visualization.arrayToDataTable(data);

  var options = {
    title: title,
  };

  var div = document.createElement('div');
  document.body.appendChild(div);

  var chart = new google.visualization.ColumnChart(div);
  chart.draw(dataTable, options);
}

function drawSeparatorLine()  {
  var hr = document.createElement('hr');
  document.body.appendChild(hr);
}

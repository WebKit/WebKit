// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
var localStreams = [];
var remoteStreams = [];

function ping(callback) {
  callback("pong");
}

function getUserMedia(constraints, onSuccessCallback, onFailCallback){
  console.log("Getting user media.");
  navigator.webkitGetUserMedia(constraints,
      onSuccessCallbackWraper, onFailCallback);

  function onSuccessCallbackWraper(stream) {
    console.log("GetUserMedia success.");
    localStreams[stream.id] = stream;
    onSuccessCallback(stream);
  }
}

function createPeerConnection(config, doneCallback, failCallback) {
  console.log("Creating peer connection");
  var obj = {};
  var pc = new webkitRTCPeerConnection(config);

  expose(obj, pc, "close");
  expose(obj, pc, "createOffer");
  expose(obj, pc, "createAnswer");
  expose(obj, pc, "addEventListener");
  expose(obj, pc, "addIceCandidate", { 0: RTCIceCandidate});
  expose(obj, pc, "setRemoteDescription", { 0: RTCSessionDescription });
  expose(obj, pc, "setLocalDescription", { 0: RTCSessionDescription });

  obj.addStream = function(stream) {
    console.log("Adding local stream.");
    var tempStream = localStreams[stream.id];
    if (!tempStream) {
      console.log("Undefined stream!");
      return;
    }
    pc.addStream(tempStream);
  };

  // Return an array of Objects, each Object is a copy of RTCStateReport
  // and has the following attributes (id, type, names, and stats).
  // names: array originaly returned by calling RTCStateReport.names().
  // stats: dictionary of stat name as key and stat value as dictionary
  // value.
  obj.getStats = function(callback, mediaTrack) {
    pc.getStats(onStatsReady, mediaTrack);

    function onStatsReady(stateResponse) {
      var outputReports = [];
      var reports = stateResponse.result();
      for (index in reports) {
        var report = {};
        report.id = reports[index].id;
        report.type = reports[index].type;
        report.names = reports[index].names();
        report.stats = [];
        populateStats(reports[index], report.stats);

        outputReports.push(report);
      }

      callback(outputReports);
    }

    function populateStats(report, stats) {
      var names = report.names();
      for (index in names) {
        stats.push({
           name: names[index],
           stat: report.stat(names[index]),
          });
      }

    }
  };

  pc.addEventListener('addstream', function(event) {
    remoteStreams[event.stream.id] = event.stream;
  });

  doneCallback(obj);
};

function showStream(streamId, autoplay, muted) {
  var stream = getStreamFromIdentifier_(streamId);
  var video = document.createElement('video');
  video.autoplay = autoplay;
  video.muted = muted;
  document.body.appendChild(video);
  video.src = URL.createObjectURL(stream);
  console.log("Stream " + stream.id + " attached to video element");
};

function getStreamFromIdentifier_(id) {
  var tempStream = localStreams[id];
  if (tempStream)
    return tempStream;
  tempStream = remoteStreams[id];
  if (tempStream)
    return tempStream;
  console.log(id + " is not id for stream.");
  return null;
};

function downloadFile(path, onSuccess, onError) {
  var xhr = new XMLHttpRequest();
  function onResult() {
    if (xhr.readyState != 4)
      return;

    if (xhr.status != 200) {
      onError("Download request failed!");
      return;
    }
    onSuccess(xhr.responseText);
  }

  xhr.onreadystatechange = onResult;
  xhr.open('GET', path, true);
  xhr.send();
};

connectToServer({
  ping: ping,
  getUserMedia: getUserMedia,
  createPeerConnection: createPeerConnection,
  showStream: showStream,
  downloadFile: downloadFile,
});

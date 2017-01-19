// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This file exposes the api for the bot to connect to the host script
// waiting a websocket connection and using dnode for javascript rpc.
//
// This file is served to the browser via browserify to resolve the
// dnode requires.
var WebSocketStream = require('websocket-stream');
var Dnode = require('dnode');

function connectToServer(api) {
  var stream = new WebSocketStream("wss://localhost:8080/");
  var dnode = new Dnode(api);
  dnode.on('error', function (error) { console.log(error); });
  dnode.pipe(stream).pipe(dnode);
}

// Dnode loses certain method calls when exposing native browser objects such as
// peer connections. This methods helps work around that by allowing one to
// redefine a non-native method in a target "obj" from "src" that applies a list
// of casts to the arguments (types are lost in dnode).
function expose(obj, src, method, casts) {
  obj[method] = function () {
    for (index in casts)
      arguments[index] = new (casts[index])(arguments[index]);
    src[method].apply(src, arguments);
  }
}

window.expose = expose;
window.connectToServer = connectToServer;

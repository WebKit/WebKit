// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Returns "not-equal" when value of index is not equal
info: |
  Atomics.wait( typedArray, index, value, timeout )

  14.If v is not equal to w, then
    a.Perform LeaveCriticalSection(WL).
    b. Return the String "not-equal".

features: [ Atomics, SharedArrayBuffer, TypedArray ]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(100);
  }
  return r;
}

$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
    
  $262.agent.report(Atomics.wait(int32Array, 0, 44, 1000));
  
  $262.agent.report(Atomics.wait(int32Array, 0, 251.4, 1000));

  $262.agent.leaving();
})
`);

var int32Array = new Int32Array(new SharedArrayBuffer(1024));

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(200);


assert.sameValue(getReport(), "not-equal");
assert.sameValue(getReport(), "not-equal");

assert.sameValue(Atomics.wake(int32Array, 0), 0);

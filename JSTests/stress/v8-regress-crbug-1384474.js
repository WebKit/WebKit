//@ requireOptions("--useResizableArrayBuffer=1")
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab

load("./resources/v8-mjsunit.js", "caller relative");
load("./resources/v8-typedarray-helpers.js", "caller relative");

const ta = new Int8Array(4);
const rab = new ArrayBuffer(10, {"maxByteLength": 20});
const lengthTracking = new Int8Array(rab);
rab.resize(0);
ta.constructor = { [Symbol.species]: function() { return lengthTracking; } };
assertThrows(() => { ta.slice(); }, TypeError);

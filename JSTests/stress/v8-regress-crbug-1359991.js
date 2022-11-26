//@ requireOptions("--useResizableArrayBuffer=1")
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2022 Apple Inc. All rights reserved.
// Modified since original V8's test had an issue.

// Flags: --harmony-rab-gsab

"use strict";

load("./resources/v8-mjsunit.js", "caller relative");
load("./resources/v8-typedarray-helpers.js", "caller relative");

const rab = new ArrayBuffer(1744, {"maxByteLength": 4000});
let callSlice = true;
class MyFloat64Array extends Float64Array {
  constructor() {
    super(rab);
    if (callSlice) {
      callSlice = false;  // Prevent recursion
      super.slice();
    }
  }
};
new MyFloat64Array();

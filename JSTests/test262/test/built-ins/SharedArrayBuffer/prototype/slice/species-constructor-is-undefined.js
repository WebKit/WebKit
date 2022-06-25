// Copyright (C) 2015 André Bargull. All rights reserved.
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Uses default constructor is `constructor` property is undefined.
info: |
  SharedArrayBuffer.prototype.slice ( start, end )
features: [SharedArrayBuffer]
---*/

var arrayBuffer = new SharedArrayBuffer(8);
arrayBuffer.constructor = undefined;

var result = arrayBuffer.slice();
assert.sameValue(Object.getPrototypeOf(result), SharedArrayBuffer.prototype);

// Copyright (C) 2015 André Bargull. All rights reserved.
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  The `end` index defaults to [[ArrayBufferByteLength]] if undefined.
info: |
  SharedArrayBuffer.prototype.slice ( start, end )

features: [SharedArrayBuffer]
---*/

var arrayBuffer = new SharedArrayBuffer(8);

var start = 6,
  end = undefined;
var result = arrayBuffer.slice(start, end);
assert.sameValue(result.byteLength, 2);

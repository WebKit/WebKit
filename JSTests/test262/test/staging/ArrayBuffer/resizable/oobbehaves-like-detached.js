// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from OOBBehavesLikeDetached test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, { maxByteLength: maxByteLength });
}

function ReadElement2(ta) {
  return ta[2];
}
function HasElement2(ta) {
  return 2 in ta;
}
const rab = CreateResizableArrayBuffer(16, 40);
const i8a = new Int8Array(rab, 0, 4);
i8a.__proto__ = { 2: 'wrong value' };
i8a[2] = 10;
assert.sameValue(ReadElement2(i8a), 10);
assert(HasElement2(i8a));
rab.resize(0);
assert.sameValue(ReadElement2(i8a), undefined);
assert(!HasElement2(i8a));

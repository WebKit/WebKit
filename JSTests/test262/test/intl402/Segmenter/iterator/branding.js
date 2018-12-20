// Copyright 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-segment-iterator-prototype
description: Verifies the behavior for the iterators.
features: [Intl.Segmenter]
---*/

let seg = new Intl.Segmenter();
let segmentIterator = seg.segment('text');
let prototype = Object.getPrototypeOf(segmentIterator);
const otherReceivers = [
    1, 123.45, undefined, null, "string", true, false,
    Intl, Intl.Segmenter, Intl.Segmenter.prototype,
    prototype,
    new Intl.Segmenter(),
    new Intl.Collator(),
    new Intl.DateTimeFormat(),
    new Intl.NumberFormat(),
];
for (const rec of otherReceivers) {
   assert.throws(TypeError, () => prototype.next.call(rec));
   assert.throws(TypeError, () => prototype.following.call(rec));
   assert.throws(TypeError, () => prototype.preceding.call(rec));
}

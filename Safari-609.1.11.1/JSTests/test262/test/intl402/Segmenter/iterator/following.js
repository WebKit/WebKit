// Copyright 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-segment-iterator-prototype
description: Verifies the behavior for the iterators.
features: [Intl.Segmenter]
---*/

const segmenter = new Intl.Segmenter();
const text = "Hello World, Test 123! Foo Bar. How are you?";
const iter = segmenter.segment(text);

assert.sameValue("function", typeof iter.following);

const tests = [
  ["3", 4],
  ["ABC", 1],
  [null, 1],
  [true, 2],
  [1.4, 2],
  [{ valueOf() { return 5; } }, 6],
  [0, 1],
  [text.length - 1, text.length],
];

for (const [input, index] of tests) {
  assert.sameValue(iter.following(input), false);
  assert.sameValue(iter.index, index, String(input));
}

assert.throws(RangeError, () => iter.following(-3));
// 1.5.3.2 %SegmentIteratorPrototype%.following( [ from ] )
// 3.b If from >= iterator.[[SegmentIteratorString]], throw a RangeError exception.
assert.throws(RangeError, () => iter.following(text.length));
assert.throws(RangeError, () => iter.following(text.length + 1));

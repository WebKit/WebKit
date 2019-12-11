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

assert.sameValue(iter.index, 0);

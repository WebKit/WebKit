// Copyright (C) 2023 Anthony Frehner and Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.symmetricdifference
description: Set.prototype.symmetricDifference throws when receiver is not a Set
features: [set-methods]
---*/

class MySetLike {
  constructor() {
    this.size = 2;
    this.has = () => {};
    this.keys = function* keys() {
      yield 2;
      yield 3;
    };
  }
}

const s1 = new MySetLike();
const s2 = new Set();
assert.throws(
  TypeError,
  () => {
    Set.prototype.symmetricDifference.call(s1, s2);
  },
  "Set-like class"
);

const s3 = {
  size: 2,
  has: () => {},
  keys: function* keys() {
    yield 2;
    yield 3;
  },
};
assert.throws(
  TypeError,
  () => {
    Set.prototype.symmetricDifference.call(s3, s2);
  },
  "Set-like object"
);

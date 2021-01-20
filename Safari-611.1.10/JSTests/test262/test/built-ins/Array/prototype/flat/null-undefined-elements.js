// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flat
description: >
    arrays with null, and undefined
includes: [compareArray.js]
features: [Array.prototype.flat]
---*/

var a = [void 0];

assert(compareArray([1, null, void 0].flat(), [1, null, undefined]));
assert(compareArray([1, [null, void 0]].flat(), [1, null, undefined]));
assert(compareArray([
  [null, void 0],
  [null, void 0]
].flat(), [null, undefined, null, undefined]));
assert(compareArray([1, [null, a]].flat(1), [1, null, a]));
assert(compareArray([1, [null, a]].flat(2), [1, null, undefined]));

// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.fill
description: >
  Fills elements from coerced to Integer `start` and `end` values
info: |
  22.1.3.6 Array.prototype.fill (value [ , start [ , end ] ] )

  ...
  7. Let relativeStart be ToInteger(start).
  8. ReturnIfAbrupt(relativeStart).
  9. If relativeStart < 0, let k be max((len + relativeStart),0); else let k be
  min(relativeStart, len).
  10. If end is undefined, let relativeEnd be len; else let relativeEnd be
  ToInteger(end).
  ...
includes: [compareArray.js]
---*/

assert.compareArray([0, 0].fill(1, undefined), [1, 1],
  '[0, 0].fill(1, undefined) must return [1, 1]'
);

assert.compareArray([0, 0].fill(1, 0, undefined), [1, 1],
  '[0, 0].fill(1, 0, undefined) must return [1, 1]'
);

assert.compareArray([0, 0].fill(1, null), [1, 1],
  '[0, 0].fill(1, null) must return [1, 1]'
);

assert.compareArray([0, 0].fill(1, 0, null), [0, 0],
  '[0, 0].fill(1, 0, null) must return [0, 0]'
);

assert.compareArray([0, 0].fill(1, true), [0, 1],
  '[0, 0].fill(1, true) must return [0, 1]'
);

assert.compareArray([0, 0].fill(1, 0, true), [1, 0],
  '[0, 0].fill(1, 0, true) must return [1, 0]'
);

assert.compareArray([0, 0].fill(1, false), [1, 1],
  '[0, 0].fill(1, false) must return [1, 1]'
);

assert.compareArray([0, 0].fill(1, 0, false), [0, 0],
  '[0, 0].fill(1, 0, false) must return [0, 0]'
);

assert.compareArray([0, 0].fill(1, NaN), [1, 1],
  '[0, 0].fill(1, NaN) must return [1, 1]'
);

assert.compareArray([0, 0].fill(1, 0, NaN), [0, 0],
  '[0, 0].fill(1, 0, NaN) must return [0, 0]'
);

assert.compareArray([0, 0].fill(1, '1'), [0, 1],
  '[0, 0].fill(1, "1") must return [0, 1]'
);

assert.compareArray([0, 0].fill(1, 0, '1'), [1, 0],
  '[0, 0].fill(1, 0, "1") must return [1, 0]'
);

assert.compareArray([0, 0].fill(1, 1.5), [0, 1],
  '[0, 0].fill(1, 1.5) must return [0, 1]'
);

assert.compareArray([0, 0].fill(1, 0, 1.5), [1, 0],
  '[0, 0].fill(1, 0, 1.5) must return [1, 0]'
);

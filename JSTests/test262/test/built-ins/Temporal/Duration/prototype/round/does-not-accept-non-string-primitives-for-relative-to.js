// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: round() does not accept non-string primitives for relativeTo
features: [Temporal]
---*/

const d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);

[
  20200101,
  20200101n,
  null,
  true,
].forEach(relativeTo => {
  assert.throws(
    TypeError, () => d.round({ smallestUnit: "seconds", relativeTo})
  );
});

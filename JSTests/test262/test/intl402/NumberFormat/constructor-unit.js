// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializenumberformat
description: Checks handling of the unit style.
features: [Intl.NumberFormat-unified]
---*/

assert.throws(TypeError, () => {
  new Intl.NumberFormat([], {
    style: "unit",
  })
});

for (const unit of [undefined, "test", "MILE", "kB"]) {
  assert.throws(unit === undefined ? TypeError : RangeError, () => {
    new Intl.NumberFormat([], {
      style: "unit",
      unit,
    })
  });

  for (const style of [undefined, "decimal", "currency"]) {
    let called = 0;
    const nf = new Intl.NumberFormat([], {
      style,
      get unit() { ++called; return unit; },
      currency: "USD",
    });
    assert.sameValue(nf.resolvedOptions().unit, undefined);
    assert.sameValue(called, 1);
  }
}

const nf = new Intl.NumberFormat([], {
  style: "percent",
});
assert.sameValue(nf.resolvedOptions().style, "percent");
assert.sameValue(nf.resolvedOptions().unit, undefined);

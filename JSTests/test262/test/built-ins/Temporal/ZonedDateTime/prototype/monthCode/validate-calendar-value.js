// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.monthcode
description: Validate result returned from calendar monthCode() method
features: [Temporal]
---*/

const badResults = [
  [undefined, TypeError],
  [Symbol("foo"), TypeError],
  [null, TypeError],
  [true, TypeError],
  [false, TypeError],
  [7.1, TypeError],
  [{toString() { return "M01"; }}, TypeError],
];

badResults.forEach(([result, error]) => {
  const calendar = new class extends Temporal.Calendar {
    monthCode() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
  assert.throws(error, () => instance.monthCode, `${typeof result} ${String(result)} not converted to string`);
});

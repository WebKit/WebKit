// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: TypeError thrown if time zone reports an id that is not a String
features: [Temporal]
---*/

class CustomTimeZone extends Temporal.TimeZone {
  constructor(id) {
    super("UTC");
    this._id = id;
  }
  get id() {
    return this._id;
  }
}

[
  undefined,
  null,
  true,
  -1000,
  Symbol(),
  3600_000_000_000n,
  {},
  {
    valueOf() {
      return 3600_000_000_000;
    }
  }
].forEach((wrongId) => {
  const timeZone = new CustomTimeZone(wrongId);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC");
  const properties = { year: 2004, month: 11, day: 9, hour: 11, minute: 33, second: 20, timeZone };
  assert.throws(TypeError, () => datetime.until(properties, { largestUnit: "days" }));
});

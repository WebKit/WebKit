// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.equals
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
  const timeZoneWrong = new CustomTimeZone(wrongId);
  const timeZoneOK = new Temporal.TimeZone('UTC');
  assert.throws(TypeError, () => timeZoneWrong.equals(timeZoneOK));
  assert.throws(TypeError, () => timeZoneOK.equals(timeZoneWrong));
});

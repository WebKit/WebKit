// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: TypeError for missing properties
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const date = new Temporal.PlainDate(2000, 5, 2);
const options = {
  get overflow() {
    TemporalHelpers.assertUnreachable("should not get overflow");
  }
};
assert.throws(TypeError, () => date.subtract({}, options), "empty object");
assert.throws(TypeError, () => date.subtract({ month: 12 }, options), "misspelled 'months'");

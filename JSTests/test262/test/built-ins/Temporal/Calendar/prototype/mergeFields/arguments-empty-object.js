// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Either argument being an empty object should result in a copy of the other object
includes: [compareArray.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");

let calls = 0;
const yearObserver = {
  get year() {
    calls++;
    return 2021;
  }
};

const result1 = calendar.mergeFields(yearObserver, {});
assert.sameValue(calls, 1, "property copied");
assert.compareArray(Object.keys(result1), ["year"]);
assert.sameValue(result1.year, 2021);
assert.sameValue(calls, 1, "result has a data property");

calls = 0;
const result2 = calendar.mergeFields({}, yearObserver);
assert.sameValue(calls, 1, "property copied");
assert.compareArray(Object.keys(result2), ["year"]);
assert.sameValue(result2.year, 2021);
assert.sameValue(calls, 1, "result has a data property");

// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetimeiso
description: Functions when time zone argument is omitted
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [];

Object.defineProperty(Temporal.TimeZone, "from", {
  get() {
    actual.push("get Temporal.TimeZone.from");
    return undefined;
  },
});

const resultExplicit = Temporal.Now.plainDateTimeISO(undefined);
assert(
  resultExplicit instanceof Temporal.PlainDateTime,
  'The result of evaluating (resultExplicit instanceof Temporal.PlainDateTime) is expected to be true'
);

assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');

const resultImplicit = Temporal.Now.plainDateTimeISO();
assert(
  resultImplicit instanceof Temporal.PlainDateTime,
  'The result of evaluating (resultImplicit instanceof Temporal.PlainDateTime) is expected to be true'
);

assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');

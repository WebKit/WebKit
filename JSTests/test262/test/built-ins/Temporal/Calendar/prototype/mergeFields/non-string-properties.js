// Copyright (C) 2021 the V8 project authors. All rights reserved.
// Copyright (C) 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Both string and symbol keys from the arguments are merged
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. Set fields to ? ToObject(fields).
  5. Set additionalFields to ? ToObject(additionalFields).
  6. Return ? DefaultMergeFields(fields, additionalFields).
features: [Temporal]
---*/

function assertEntriesEqual(actual, expectedEntries, message) {
  const names = Object.getOwnPropertyNames(actual);
  const symbols = Object.getOwnPropertySymbols(actual);
  const actualKeys = names.concat(symbols);
  assert.sameValue(
    actualKeys.length,
    expectedEntries.length,
    `${message}: expected object to have ${expectedEntries.length} properties, not ${actualKeys.length}:`
  );
  for (var index = 0; index < actualKeys.length; index++) {
    const actualKey = actualKeys[index];
    const expectedKey = expectedEntries[index][0];
    const expectedValue = expectedEntries[index][1];
    assert.sameValue(actualKey, expectedKey, `${message}: key ${index}:`);
    assert.sameValue(actual[actualKey], expectedValue, `${message}: value ${index}:`);
  }
}

const cal = new Temporal.Calendar("iso8601");

assertEntriesEqual(
  cal.mergeFields({ 1: 2 }, { 3: 4 }),
  [["1", 2], ["3", 4]],
  "number keys are actually string keys and are merged as such"
);
assertEntriesEqual(
  cal.mergeFields({ 1n: 2 }, { 2n: 4 }),
  [["1", 2], ["2", 4]],
  "bigint keys are actually string keys and are merged as such"
);

const foo = Symbol("foo");
const bar = Symbol("bar");
assertEntriesEqual(
  cal.mergeFields({ [foo]: 1 }, { [bar]: 2 }),
  [[foo, 1], [bar, 2]],
  "symbol keys are also merged"
);

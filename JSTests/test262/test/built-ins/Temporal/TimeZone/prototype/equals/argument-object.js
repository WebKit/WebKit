// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.equals
description: Tests that objects can be compared for equality
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

const objectsEqualUTC = [
  new Temporal.TimeZone("UTC"),
  new CustomTimeZone("UTC"),
  { id: "UTC", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  new Temporal.ZonedDateTime(0n, "UTC")
];

const tzUTC = new Temporal.TimeZone("UTC");

for (const object of objectsEqualUTC) {
  const result = tzUTC.equals(object);
  assert.sameValue(result, true, `Receiver ${tzUTC.id} should equal argument ${object.id}`);
}

const objectsEqual0000 = [
  new Temporal.TimeZone("+00:00"),
  new Temporal.TimeZone("+0000"),
  new Temporal.TimeZone("+00"),
  new CustomTimeZone("+00:00"),
  new CustomTimeZone("+0000"),
  new CustomTimeZone("+00"),
  { id: "+00:00", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  { id: "+0000", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  { id: "+00", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  new Temporal.ZonedDateTime(0n, "+00:00"),
  new Temporal.ZonedDateTime(0n, "+0000"),
  new Temporal.ZonedDateTime(0n, "+00"),
  "+00:00",
  "+0000",
  "+00"
];

const tz0000ToTest = [
  new Temporal.TimeZone("+00:00"),
  new Temporal.TimeZone("+0000"),
  new Temporal.TimeZone("+00"),
  new CustomTimeZone("+00:00"),
  new CustomTimeZone("+0000"),
  new CustomTimeZone("+00")
];

for (const arg of objectsEqual0000) {
  for (const receiver of tz0000ToTest) {
    const result = receiver.equals(arg);
    assert.sameValue(result, true, `Receiver ${receiver.id} should equal argument ${arg.id ?? arg}`);
  }
}

const objectsNotEqual = [
  new Temporal.TimeZone("+00:00"),
  new CustomTimeZone("+00:00"),
  new CustomTimeZone("Etc/Custom"),
  { id: "+00:00", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  { id: "Etc/Custom", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null },
  new Temporal.ZonedDateTime(0n, "+00:00"),
  "UTC"
];

const customObjectsToTest = [tzUTC, new CustomTimeZone("YouTeeSee"), new CustomTimeZone("+01:00")];

for (const arg of objectsNotEqual) {
  for (const receiver of customObjectsToTest) {
    if (arg === "UTC" && receiver === tzUTC) continue;
    const result = receiver.equals(arg);
    assert.sameValue(result, false, `Receiver ${receiver.id} should not equal argument ${arg.id ?? arg}`);
  }
}

// Custom object IDs are compared case-sensitively
const classInstanceCustomId = new CustomTimeZone("Moon/Cheese");
const classInstanceSameCaseCustomId = new CustomTimeZone("Moon/Cheese");
const classInstanceDifferentCaseCustomId = new CustomTimeZone("MoOn/CHEESe");

const plainObjectSameCaseCustomId = { id: "Moon/Cheese", getPossibleInstantsFor: null, getOffsetNanosecondsFor: null };
const plainObjectDifferentCaseCustomId = {
  id: "MoOn/CHEESe",
  getPossibleInstantsFor: null,
  getOffsetNanosecondsFor: null
};

assert.sameValue(classInstanceCustomId.equals(classInstanceSameCaseCustomId), true);
assert.sameValue(classInstanceCustomId.equals(classInstanceDifferentCaseCustomId), false);
assert.sameValue(classInstanceCustomId.equals(plainObjectSameCaseCustomId), true);
assert.sameValue(classInstanceCustomId.equals(plainObjectDifferentCaseCustomId), false);

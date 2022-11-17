// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: >
  When calendar.fields is undefined, with() doesn't perform an
  observable array iteration
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
calendar.fields = undefined;

const timeZone = new class extends Temporal.TimeZone {
  getPossibleInstantsFor(dt) {
    const instants = super.getPossibleInstantsFor.call(this, dt);
    // Return an iterable object that doesn't call Array iterator
    return {
      *[Symbol.iterator]() {
        for (let index = 0; index < instants.length; index++) {
          yield instants[index];
        }
      }
    }
  }
}("UTC");

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone, calendar);

// Detect observable array iteration:
const oldIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () {
  throw new Test262Error(`array shouldn't be iterated: ${new Error().stack}`);
};

instance.with({ year: 2022 });

Array.prototype[Symbol.iterator] = oldIterator;

// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: Checks the propagation of exceptions from the options for the DateTimeFormat constructor.
features: [Intl.DateTimeFormat-dayPeriod]
---*/

function CustomError() {}

const options = [
  "dayPeriod",
];

for (const option of options) {
  assert.throws(CustomError, () => {
    new Intl.DateTimeFormat("en", {
      get [option]() {
        throw new CustomError();
      }
    });
  }, `Exception from ${option} getter should be propagated`);
}

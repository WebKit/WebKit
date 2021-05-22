// Copyright (C) 2021 Chengzhong Wu. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: InstallErrorCause on abrupt completions
info: |
  Error ( message [ , options ] )

  ...
  4. Perform ? InstallErrorCause(O, options).
  ...

  20.5.8.1 InstallErrorCause ( O, options )

  1. If Type(options) is Object and ? HasProperty(options, "cause") is true, then
    a. Let cause be ? Get(options, "cause").
    b. Perform ! CreateNonEnumerableDataPropertyOrThrow(O, "cause", cause).
  ...

esid: sec-error-message
features: [error-cause]
---*/

var message = "my-message";
//////////////////////////////////////////////////////////////////////////////
// CHECK#0
assert.throws(Test262Error, function () {
  var options = new Proxy({}, {
    has(target, prop) {
      if (prop === "cause") {
        throw new Test262Error("HasProperty");
      }
      return prop in target;
    },
  });
  var error = new Error(message, options);
}, "HasProperty");
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#1
assert.throws(Test262Error, function () {
  var options = {
    get cause() {
      throw new Test262Error("Get Cause");
    },
  };
  var error = new Error(message, options);
}, "Get Cause");
//////////////////////////////////////////////////////////////////////////////

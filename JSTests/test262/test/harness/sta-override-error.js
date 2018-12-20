// Copyright (c) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Including sta.js will expose two functions:

        Test262Error
        $ERROR

    Assert that global $ERROR is overridable
---*/
function BaloneyError() {}

// Override $ERROR
$ERROR = function() {
  throw new BaloneyError();
};

assert.throws(BaloneyError, function() {
  $ERROR();
});

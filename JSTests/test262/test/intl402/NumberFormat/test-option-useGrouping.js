// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.1.1_34
description: Tests that the option useGrouping is processed correctly.
info: |
  The "Intl.NumberFormat v3" proposal contradicts the behavior required by the
  latest revision of ECMA402. Likewise, this test contradicts
  test-option-useGrouping-extended.js. Until the proposal is included in a
  published standard (when the tests' discrepancies can be resolved),
  implementations should only expect to pass one of these two tests.
author: Norbert Lindenberg
includes: [testIntl.js]
---*/

testOption(Intl.NumberFormat, "useGrouping", "boolean", undefined, true);

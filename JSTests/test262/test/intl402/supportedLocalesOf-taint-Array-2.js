// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 9.2.6_2
description: >
    Tests that the behavior of a List is not affected by adversarial
    changes to Array.prototype.
author: Norbert Lindenberg
includes: [testIntl.js]
---*/

taintArray();

testWithIntlConstructors(function (Constructor) {
    // this test should work equally for both matching algorithms
    ["lookup", "best fit"].forEach(function (matcher) {
        var defaultLocale = new Constructor().resolvedOptions().locale;
        var canonicalized = Constructor.supportedLocalesOf([defaultLocale, defaultLocale],
            {localeMatcher: matcher});
        assert.sameValue(canonicalized.length > 1, false, "Canonicalization with matcher " + matcher + " didn't remove duplicate language tags from locale list.");
    });
});

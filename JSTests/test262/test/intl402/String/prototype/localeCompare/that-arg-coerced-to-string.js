// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 13.1.1_3_1
description: Tests that localeCompare coerces that to a string.
author: Norbert Lindenberg
---*/

var thisValues = ["true", "5", "hello", "good bye"];
var thatValues = [true, 5, "hello", {toString: function () { return "good bye"; }}];

var i;
for (i = 0; i < thisValues.length; i++) {
    var j;
    for (j = 0; j < thatValues.length; j++) {
        var result = String.prototype.localeCompare.call(thisValues[i], thatValues[j]);
        assert.sameValue((result === 0), (i === j), "localeCompare treats " + thisValues[i] + " and " + thatValues[j] + " as " + (result === 0 ? "equal" : "different") + ".");
    }
}

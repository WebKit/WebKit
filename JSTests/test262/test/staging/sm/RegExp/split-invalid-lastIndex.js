// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1263851;
var summary = "RegExp.prototype[@@split] should handle if lastIndex is out of bound.";

print(BUGNUMBER + ": " + summary);

var myRegExp = {
    get constructor() {
        return {
            get [Symbol.species]() {
                return function() {
                    return {
                        get lastIndex() {
                            return 9;
                        },
                        set lastIndex(v) {},
                        exec() {
                            return [];
                        }
                    };
                };
            }
        };
    }
};
var result = RegExp.prototype[Symbol.split].call(myRegExp, "abcde");;
assert.sameValue(result.length, 2);
assert.sameValue(result[0], "");
assert.sameValue(result[1], "");


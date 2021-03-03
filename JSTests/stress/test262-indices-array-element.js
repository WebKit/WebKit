// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: A matching element of indices is an Array with exactly two number properties.
esid: sec-getmatchindicesarray
features: [regexp-match-indices]
info: |
  GetMatchIndicesArray ( S, match )
    5. Return CreateArrayFromList(« _match_.[[StartIndex]], _match_.[[EndIndex]] »).
---*/

function assertSameValue(a, b)
{
    if (a !== b)
        throw "Values not same";
}

let input = "abcd";
let match = /b(c)/d.exec(input);
let indices = match.indices;

// `indices[0]` is an array
assertSameValue(Object.getPrototypeOf(indices[0]), Array.prototype);
assertSameValue(indices[0].length, 2);
assertSameValue(typeof indices[0][0], "number");
assertSameValue(typeof indices[0][1], "number");

// `indices[1]` is an array
assertSameValue(Object.getPrototypeOf(indices[1]), Array.prototype);
assertSameValue(indices[1].length, 2);
assertSameValue(typeof indices[1][0], "number");
assertSameValue(typeof indices[1][1], "number");

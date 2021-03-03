// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: An unmatched capture in a match corresponds to an unmatched capture in "indices"
esid: sec-makeindicesarray
features: [regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames )
    4. Let _n_ be the number of elements in _indices_.
    ...
    6. Set _A_ to ! ArrayCreate(_n_).
    ...
    11. For each integer _i_ such that _i_ >= 0 and _i_ < _n_, do
      a. Let _matchIndices_ be _indices_[_i_].
      b. If _matchIndices_ is not *undefined*, then
        i. Let _matchIndicesArray_ be ! GetMatchIndicesArray(_S_, _matchIndices_).
      c. Else,
        i. Let _matchIndicesArray_ be *undefined*.
      d. Perform ! CreateDataProperty(_A_, ! ToString(_n_), _matchIndicesArray_).
        ...
---*/

function assertSameValue(a, b)
{
    if (a !== b)
        throw "Values not same";
}

let input = "abd";
let match = /b(c)?/d.exec(input);
let indices = match.indices;

// `indices` has the same length as match
assertSameValue(indices.length, match.length);

// The second element of `indices` should be undefined.
assertSameValue(indices[1], undefined);

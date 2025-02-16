// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTestfile = "set-same-buffer-different-source-target-types.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 896116;
var summary =
  "When setting a typed array from an overlapping typed array of different " +
  "element type, copy the source elements into properly-sized temporary " +
  "memory, and properly copy them into the target without overflow (of " +
  "either source *or* target) when finished";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// smallest 2**n triggering segfaults in a pre-patch build locally, then
// quadrupled in case the boundary ever changes, or is different in some other
// memory-allocating implementation
var srclen = 65536;

var ta = new Uint8Array(srclen * Float64Array.BYTES_PER_ELEMENT);
var ta2 = new Float64Array(ta.buffer, 0, srclen);
ta.set(ta2);

// This test mostly exists to check for no crash above, but it's worth testing
// for no uninitialized memory (in case of buffer overflow) being copied into
// the array, too.
for (var i = 0, len = ta.length; i < len; i++)
  assert.sameValue(ta[i], 0, "zero-bits double should convert to zero");

/******************************************************************************/

print("Tests complete");

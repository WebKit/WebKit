/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 1596706;
var summary =
  "Properly apply a directive comment that's only tokenized by a syntax " +
  "parser (because the directive comment appears immediately after an arrow " +
  "function with expression body)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var stack;

function reset()
{
  stack = "";
}

function assertStackContains(needle, msg)
{
  assert.sameValue(stack.indexOf(needle) >= 0, true,
           `stack should contain '${needle}': ${msg}`);
}

Object.defineProperty(this, "detectSourceURL", {
  get() {
    stack = new Error().stack;
    return 17;
  }
});

// block followed by semicolon
reset();
assert.sameValue(eval(`x=>{};
//# sourceURL=http://example.com/foo.js
detectSourceURL`), 17);
assertStackContains("http://example.com/foo.js", "block, semi");

// block not followed by semicolon
reset();
assert.sameValue(eval(`x=>{}
//# sourceURL=http://example.com/bar.js
detectSourceURL`), 17);
assertStackContains("http://example.com/bar.js", "block, not semi");

// expr followed by semicolon
reset();
assert.sameValue(eval(`x=>y;
//# sourceURL=http://example.com/baz.js
detectSourceURL`), 17);
assertStackContains("http://example.com/baz.js", "expr, semi");

// expr not followed by semicolon
reset();
assert.sameValue(eval(`x=>y
//# sourceURL=http://example.com/quux.js
detectSourceURL`), 17);
assertStackContains("http://example.com/quux.js", "expr, not semi");

/******************************************************************************/

print("Tests complete");

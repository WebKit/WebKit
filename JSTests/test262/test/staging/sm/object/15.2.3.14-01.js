/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 307791;
var summary = 'ES5 Object.keys(O)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus(summary);

function arraysEqual(a1, a2)
{
  return a1.length === a2.length &&
         a1.every(function(v, i) { return v === a2[i]; });
}

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(Object.keys.length, 1);

var o, keys;

o = { a: 3, b: 2 };
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["a", "b"]), true,
         "" + keys);

o = { get a() { return 17; }, b: 2 };
keys = Object.keys(o),
assert.sameValue(arraysEqual(keys, ["a", "b"]), true,
         "" + keys);

o = { __iterator__: function() { throw new Error("non-standard __iterator__ called?"); } };
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["__iterator__"]), true,
         "" + keys);

o = { a: 1, b: 2 };
delete o.a;
o.a = 3;
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["b", "a"]), true,
         "" + keys);

o = [0, 1, 2];
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["0", "1", "2"]), true,
         "" + keys);

o = /./.exec("abc");
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["0", "index", "input", "groups"]), true,
         "" + keys);

o = { a: 1, b: 2, c: 3 };
delete o.b;
o.b = 5;
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["a", "c", "b"]), true,
         "" + keys);

function f() { }
f.prototype.p = 1;
o = new f();
o.g = 1;
keys = Object.keys(o);
assert.sameValue(arraysEqual(keys, ["g"]), true,
         "" + keys);

if (typeof Namespace !== "undefined" && typeof QName !== "undefined")
{
  var o2 = {};
  var qn = new QName(new Namespace("foo"), "v");
  o2.f = 1;
  o2[qn] = 3;
  o2.baz = 4;
  var keys2 = Object.keys(o2);
  assert.sameValue(arraysEqual(keys2, ["f", "foo::v", "baz"]), true,
           "" + keys2);
}

/******************************************************************************/

assert.sameValue(expect, actual, "Object.keys");

printStatus("All tests passed!");

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
var gTestfile = 'proxy-__proto__.js';
var BUGNUMBER = 950407;
var summary = "Behavior of __proto__ on ES6 proxies";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var protoDesc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
var protoGetter = protoDesc.get;
var protoSetter = protoDesc.set;

function testProxy(target, initialProto)
{
  print("Now testing behavior for new Proxy(" + ("" + target) + ", {})");

  var pobj = new Proxy(target, {});

  // Check [[Prototype]] before attempted mutation
  assert.sameValue(Object.getPrototypeOf(pobj), initialProto);
  assert.sameValue(protoGetter.call(pobj), initialProto);

  // Attempt [[Prototype]] mutation
  protoSetter.call(pobj, null);

  // Check [[Prototype]] after attempted mutation
  assert.sameValue(Object.getPrototypeOf(pobj), null);
  assert.sameValue(protoGetter.call(pobj), null);
  assert.sameValue(Object.getPrototypeOf(target), null);
}

// Proxy object with non-null [[Prototype]]
var nonNullProto = { toString: function() { return "non-null prototype"; } };
var target = Object.create(nonNullProto);
testProxy(target, nonNullProto);

// Proxy object with null [[Prototype]]
target = Object.create(null);
target.toString = function() { return "null prototype" };
testProxy(target, null);

// Proxy function with [[Call]]
var callForCallOnly = function () { };
callForCallOnly.toString = function() { return "callable target"; };
testProxy(callForCallOnly, Function.prototype);

/******************************************************************************/

print("Tests complete");

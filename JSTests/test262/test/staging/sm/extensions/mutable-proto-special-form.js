/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 948583;
var summary =
  "Make __proto__ in object literals a special form not influenced by " +
  "|Object.prototype|";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var passed = true;

function performProtoTests(msg)
{
  print("Testing " + msg);
  assert.sameValue(passed, true, "passed wrong at start of test set");

  assert.sameValue(Object.getPrototypeOf({ __proto__: null }), null);
  assert.sameValue(Object.getPrototypeOf({ __proto__: undefined }), Object.prototype);
  assert.sameValue(Object.getPrototypeOf({ __proto__: 17 }), Object.prototype);

  var obj = {};
  assert.sameValue(Object.getPrototypeOf({ __proto__: obj }), obj);

  assert.sameValue(passed, true, "passed wrong at end of test set");
  print("Tests of " + msg + " passed!");
}

function poisonProto(obj)
{
  Object.defineProperty(obj, "__proto__",
                        {
                          configurable: true,
                          enumerable: true,
                          set: function(v) { passed = false; },
                        });
}

performProtoTests("initial behavior");

var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
var setProto = desc.set;
delete Object.prototype.__proto__;

performProtoTests("behavior after Object.prototype.__proto__ deletion");

Object.defineProperty(Object.prototype, "__proto__",
                      {
                        configurable: true,
                        enumerable: false,
                        set: function(v) { passed = false; },
                      });

performProtoTests("behavior after making Object.prototype.__proto__ a " +
                  "custom setter");

Object.defineProperty(Object.prototype, "__proto__", { set: undefined });

performProtoTests("behavior after making Object.prototype.__proto__'s " +
                  "[[Set]] === undefined");

try
{
  var superProto = Object.create(null);
  poisonProto(superProto);
  setProto.call(Object.prototype, superProto);

  performProtoTests("behavior after mutating Object.prototype.[[Prototype]]");

  // Note: The handler below will have to be updated to exempt a scriptable
  //       getPrototypeOf trap (to instead consult the target whose [[Prototype]]
  //       is safely non-recursive), if we ever implement one.
  var death = new Proxy(Object.create(null),
                        new Proxy({}, { get: function() { passed = false; } }));

  setProto.call(Object.prototype, death);

  performProtoTests("behavior after making Object.prototype.[[Prototype]] a " +
                    "proxy that throws for any access");
}
catch (e) {}

print("Tests complete");

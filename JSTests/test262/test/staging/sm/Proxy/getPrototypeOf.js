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
var gTestfile = "getPrototypeOf.js";
var BUGNUMBER = 888969;
var summary = "Scripted proxies' [[GetPrototypeOf]] behavior";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

const log = [];

function observe(obj)
{
  var observingHandler = new Proxy({}, {
    get(target, p, receiver) {
      log.push(p);
      return Reflect.get(target, p, receiver);
    }
  });

  return new Proxy(obj, observingHandler);
}

function nop() {}

var p, h;

// 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
// 2. If handler is null, throw a TypeError exception.
// 3. Assert: Type(handler) is Object.
var rev = Proxy.revocable({}, {});
p = rev.proxy;

assert.sameValue(Object.getPrototypeOf(p), Object.prototype);
rev.revoke();
assertThrowsInstanceOf(() => Object.getPrototypeOf(p), TypeError);

// 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
// 5. Let trap be ? GetMethod(handler, "getPrototypeOf").

// Getting handler.getPrototypeOf might throw.
assertThrowsValue(() => Object.getPrototypeOf(new Proxy({},
                                                        { get getPrototypeOf() {
                                                            throw 17;
                                                          } })),
                  17);

// The handler's getPrototypeOf, once gotten, might throw.
p = new Proxy({}, { getPrototypeOf() { throw 42; } });

assertThrowsValue(() => Object.getPrototypeOf(p), 42);

// The trap might not be callable.
p = new Proxy({}, { getPrototypeOf: 17 });

assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                       TypeError);

// 6. If trap is undefined, then
//    a. Return ? target.[[GetPrototypeOf]]().

var x, tp;

tp =
  new Proxy(new Number(8675309), // behavior overridden by getPrototypeOf
            { getPrototypeOf() { x = "getPrototypeOf trap"; return null; } });

// The target's [[GetPrototypeOf]] should be invoked if the handler's
// .getPrototypeOf is undefined.
p = new Proxy(tp, { getPrototypeOf: undefined });
x = "unset";
assert.sameValue(Object.getPrototypeOf(p), null);
assert.sameValue(x, "getPrototypeOf trap");

// Likewise if the handler's .getPrototypeOf is null.
p = new Proxy(tp, { getPrototypeOf: null });
x = "unset";
assert.sameValue(Object.getPrototypeOf(p), null);
assert.sameValue(x, "getPrototypeOf trap");

// Now the target is an empty object with a Number object as its [[Prototype]].
var customProto = new Number(8675309);
tp =
  new Proxy({},
            { getPrototypeOf() {
                x = "getPrototypeOf trap";
                return customProto;
              } });

// The target's [[GetPrototypeOf]] should be invoked if the handler's
// .getPrototypeOf is undefined.
p = new Proxy(tp, { getPrototypeOf: undefined });
x = "unset";
assert.sameValue(Object.getPrototypeOf(p), customProto);
assert.sameValue(x, "getPrototypeOf trap");

// Likewise if the handler's .getPrototypeOf is null.
p = new Proxy(tp, { getPrototypeOf: null });
x = "unset";
assert.sameValue(Object.getPrototypeOf(p), customProto);
assert.sameValue(x, "getPrototypeOf trap");

// 7. Let handlerProto be ? Call(trap, handler, « target »).

// The trap callable might throw.
p = new Proxy({}, { getPrototypeOf() { throw "ohai"; } });

assertThrowsValue(() => Object.getPrototypeOf(p),
                  "ohai");

var throwingTrap =
  new Proxy(function() { throw "not called"; },
            { apply() { throw 37; } });

p = new Proxy({}, { getPrototypeOf: throwingTrap });

assertThrowsValue(() => Object.getPrototypeOf(p),
                  37);

// The trap callable must *only* be called.
p = new Proxy({},
              {
                getPrototypeOf: observe(function() { throw "boo-urns"; })
              });

log.length = 0;
assertThrowsValue(() => Object.getPrototypeOf(p),
                  "boo-urns");

assert.sameValue(log.length, 1);
assert.sameValue(log[0], "apply");

// 8. If Type(handlerProto) is neither Object nor Null, throw a TypeError exception.

var rval;

var typeTestingTarget = {};
p = new Proxy(typeTestingTarget, { getPrototypeOf() { return rval; } });

function returnsPrimitives()
{
  rval = undefined;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = true;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = false;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = 0.0;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = -0.0;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = 3.141592654;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = NaN;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = -Infinity;
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = "[[Prototype]] FOR REALZ";
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);

  rval = Symbol("[[Prototype]] FOR REALZ");
  assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                         TypeError);
}

returnsPrimitives();
Object.preventExtensions(typeTestingTarget);
returnsPrimitives();

// 9. Let extensibleTarget be ? IsExtensible(target).

var act, extens;

var typeTestingProxyTarget =
  new Proxy({}, { isExtensible() {
                    seen = act();
                    return extens;
                  } });

p = new Proxy(typeTestingProxyTarget, { getPrototypeOf() { return rval; } });

rval = null;
act = () => { throw "fnord" };
assertThrowsValue(() => Object.getPrototypeOf(p),
                  "fnord");

rval = /abc/;
act = () => { throw "fnord again" };
assertThrowsValue(() => Object.getPrototypeOf(p),
                  "fnord again");

rval = Object.prototype;
act = () => { throw "fnord" };
assertThrowsValue(() => Object.getPrototypeOf(p),
                  "fnord");

// 10. If extensibleTarget is true, return handlerProto.

p = new Proxy({}, { getPrototypeOf() { return rval; } });

rval = Number.prototype;
assert.sameValue(Object.getPrototypeOf(p), Number.prototype);

// 11. Let targetProto be ? target.[[GetPrototypeOf]]().

var targetProto;

var targetWithProto =
  new Proxy(Object.preventExtensions(Object.create(null)),
            { getPrototypeOf() { act2(); return targetProto; } });

p = new Proxy(targetWithProto,
              { getPrototypeOf() { act1(); return rval; } });

rval = null;
targetProto = null;

var regex = /targetProto/;

act1 = () => log.push("act1");
act2 = () => log.push("act2");

log.length = 0;
assert.sameValue(Object.getPrototypeOf(p), null);
assert.sameValue(log.length, 2);
assert.sameValue(log[0], "act1");
assert.sameValue(log[1], "act2");

act1 = () => log.push("act1 again");
act2 = () => { throw "target throw"; };

log.length = 0;
assertThrowsValue(() => Object.getPrototypeOf(p),
                  "target throw");
assert.sameValue(log.length, 1);
assert.sameValue(log[0], "act1 again");

// 12. If SameValue(handlerProto, targetProto) is false, throw a TypeError exception.

act1 = act2 = nop;
rval = /a/;
assertThrowsInstanceOf(() => Object.getPrototypeOf(p),
                       TypeError);

// 13. Return handlerProto.

rval = null;
targetProto = null;

assert.sameValue(Object.getPrototypeOf(p), null);

p = new Proxy(Object.preventExtensions(new Number(55)),
              { getPrototypeOf() { return Number.prototype; } });

assert.sameValue(Object.getPrototypeOf(p), Number.prototype);

/******************************************************************************/

print("Tests complete");

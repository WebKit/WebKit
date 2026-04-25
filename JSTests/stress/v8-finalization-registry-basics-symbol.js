// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs

load("./resources/v8-mjsunit.js", "caller relative");

(function TestConstructFinalizationRegistry() {
  let fg = new FinalizationRegistry(() => {});
  assertEquals(fg.toString(), "[object FinalizationRegistry]");
  assertNotSame(fg.__proto__, Object.prototype);
  assertSame(fg.__proto__.__proto__, Object.prototype);
})();

(function TestFinalizationRegistryConstructorCallAsFunction() {
  let caught = false;
  let message = "";
  try {
    let f = FinalizationRegistry(() => {});
  } catch (e) {
    message = e.message;
    caught = true;
  } finally {
    assertTrue(caught);
  }
})();

(function TestConstructFinalizationRegistryCleanupNotCallable() {
  let message = "FinalizationRegistry: cleanup must be callable";
  assertThrows(() => { let fg = new FinalizationRegistry(); }, TypeError, message);
  assertThrows(() => { let fg = new FinalizationRegistry(1); }, TypeError, message);
  assertThrows(() => { let fg = new FinalizationRegistry(null); }, TypeError, message);
})();

(function TestConstructFinalizationRegistryWithCallableProxyAsCleanup() {
  let handler = {};
  let obj = () => {};
  let proxy = new Proxy(obj, handler);
  let fg = new FinalizationRegistry(proxy);
})();

(function TestConstructFinalizationRegistryWithNonCallableProxyAsCleanup() {
  let message = "FinalizationRegistry: cleanup must be callable";
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  assertThrows(() => { let fg = new FinalizationRegistry(proxy); }, TypeError, message);
})();

(function TestRegisterWithNonObjectTarget() {
  let fg = new FinalizationRegistry(() => {});
  let message = "FinalizationRegistry.prototype.register: target must be an object";
  assertThrows(() => fg.register(1, "holdings"), TypeError, message);
  assertThrows(() => fg.register(false, "holdings"), TypeError, message);
  assertThrows(() => fg.register("foo", "holdings"), TypeError, message);
  assertThrows(() => fg.register(null, "holdings"), TypeError, message);
  assertThrows(() => fg.register(undefined, "holdings"), TypeError, message);
})();

(function TestRegisterWithProxy() {
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  let fg = new FinalizationRegistry(() => {});
  fg.register(proxy);
})();

(function TestRegisterTargetAndHoldingsSameValue() {
  let fg = new FinalizationRegistry(() => {});
  let sym = Symbol();
  // SameValue(target, holdings) not ok
  assertThrows(() => fg.register(sym, sym), TypeError,
               "FinalizationRegistry.prototype.register: target and holdings must not be same");
  let holdings = Symbol();
  fg.register(sym, holdings);
})();

(function TestRegisterWithoutFinalizationRegistry() {
  assertThrows(() => FinalizationRegistry.prototype.register.call({}, Symbol(), "holdings"), TypeError);
  // Does not throw:
  let fg = new FinalizationRegistry(() => {});
  FinalizationRegistry.prototype.register.call(fg, Symbol(), "holdings");
})();

(function TestUnregisterWithNonExistentKey() {
  let fg = new FinalizationRegistry(() => {});
  let success = fg.unregister(Symbol());
  assertFalse(success);
})();

(function TestUnregisterWithNonFinalizationRegistry() {
  assertThrows(() => FinalizationRegistry.prototype.unregister.call({}, {}),
               TypeError);
})();

(function TestUnregisterWithNonObjectUnregisterToken() {
  let fg = new FinalizationRegistry(() => {});
  assertThrows(() => fg.unregister(1), TypeError);
  assertThrows(() => fg.unregister(BigInt(1)), TypeError);
  assertThrows(() => fg.unregister('one'), TypeError);
  assertThrows(() => fg.unregister(true), TypeError);
  assertThrows(() => fg.unregister(false), TypeError);
  assertThrows(() => fg.unregister(undefined), TypeError);
  assertThrows(() => fg.unregister(null), TypeError);
})();

(function TestWeakRefConstructor() {
  let wr = new WeakRef(Symbol());
  assertEquals(wr.toString(), "[object WeakRef]");
  assertNotSame(wr.__proto__, Object.prototype);

  let deref_desc = Object.getOwnPropertyDescriptor(wr.__proto__, "deref");
  assertEquals(true, deref_desc.configurable);
  assertEquals(false, deref_desc.enumerable);
  assertEquals("function", typeof deref_desc.value);
})();

(function TestWeakRefConstructorWithNonObject() {
  let message = "WeakRef: target must be an object";
  assertThrows(() => new WeakRef(), TypeError, message);
  assertThrows(() => new WeakRef(1), TypeError, message);
  assertThrows(() => new WeakRef(false), TypeError, message);
  assertThrows(() => new WeakRef("foo"), TypeError, message);
  assertThrows(() => new WeakRef(null), TypeError, message);
  assertThrows(() => new WeakRef(undefined), TypeError, message);
})();

(function TestWeakRefConstructorCallAsFunction() {
  let caught = false;
  let message = "";
  try {
    let f = WeakRef(Symbol());
  } catch (e) {
    message = e.message;
    caught = true;
  } finally {
    assertTrue(caught);
  }
})();

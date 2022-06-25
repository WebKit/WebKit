// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");
let Realm = { create: $.createRealm, eval: (r, s) => $.evalScript.call(r, s)  };

let r = Realm.create();

let cleanup = Realm.eval(r, "var stored_global; function cleanup() { stored_global = globalThis; } cleanup;");
let realm_global_this = Realm.eval(r, "globalThis");

let fg = new FinalizationRegistry(cleanup);

// Create an object and a register it in the FinalizationRegistry. The object needs
// to be inside a closure so that we can reliably kill them!

(function() {
    for (let i = 0; i < 1000; ++i) {
        let object = {};
        fg.register(object);
    }
})();

gc();

// Assert that the cleanup function was called in its Realm.
let timeout_func = function() {
  let stored_global = Realm.eval(r, "stored_global;");
  assertNotEquals(stored_global, globalThis);
  assertEquals(stored_global, realm_global_this);
}

setTimeout(timeout_func, 0);

// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1151149;
var summary = "Proxy constructor should not throw if either the target or handler is a revoked proxy.";

print(BUGNUMBER + ": " + summary);

var p = new Proxy({}, {});

new Proxy(p, {});
new Proxy({}, p);

var r = Proxy.revocable({}, {});
p = r.proxy;

new Proxy(p, {});
new Proxy({}, p);

r.revoke();

new Proxy(p, {});
new Proxy({}, p);


var r2 = Proxy.revocable({}, {});
r = Proxy.revocable(r2.proxy, {});
p = r.proxy;

new Proxy(p, {});
new Proxy({}, p);

r2.revoke();

new Proxy(p, {});
new Proxy({}, p);

r.revoke();

new Proxy(p, {});
new Proxy({}, p);


var g = createNewGlobal();
p = g.eval(`var r = Proxy.revocable({}, {}); r.proxy;`);

new Proxy(p, {});
new Proxy({}, p);

g.eval(`r.revoke();`);

new Proxy(p, {});
new Proxy({}, p);


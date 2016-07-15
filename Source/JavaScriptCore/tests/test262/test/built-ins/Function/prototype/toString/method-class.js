// Copyright (C) 2016 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-runtime-semantics-definemethod
description: Function.prototype.toString on a method (class)
---*/

let f = class { /* before */f /* a */ ( /* b */ ) /* c */ { /* d */ }/* after */ }.prototype.f;
let g = class { /* before */[ /* a */ "g" /* b */ ] /* c */ ( /* d */ ) /* e */ { /* f */ }/* after */ }.prototype.g;

assert.sameValue(f.toString(), "f /* a */ ( /* b */ ) /* c */ { /* d */ }");
assert.sameValue(g.toString(), "[ /* a */ \"g\" /* b */ ] /* c */ ( /* d */ ) /* e */ { /* f */ }");

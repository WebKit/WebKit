// Copyright (C) 2016 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-generator-function-definitions-runtime-semantics-propertydefinitionevaluation
description: Function.prototype.toString on a generator method
---*/

let f = { /* before */* /* a */ f /* b */ ( /* c */ ) /* d */ { /* e */ }/* after */ }.f;
let g = { /* before */* /* a */ [ /* b */ "g" /* c */ ] /* d */ ( /* e */ ) /* f */ { /* g */ }/* after */ }.g;

assert.sameValue(f.toString(), "* /* a */ f /* b */ ( /* c */ ) /* d */ { /* e */ }");
assert.sameValue(g.toString(), "* /* a */ [ /* b */ \"g\" /* c */ ] /* d */ ( /* e */ ) /* f */ { /* g */ }");

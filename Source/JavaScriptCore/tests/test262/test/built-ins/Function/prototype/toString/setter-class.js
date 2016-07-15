// Copyright (C) 2016 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-method-definitions-runtime-semantics-propertydefinitionevaluation
description: Function.prototype.toString on a setter (class)
---*/

let f = Object.getOwnPropertyDescriptor(class { /* before */set /* a */ f /* b */ ( /* c */ a /* d */ ) /* e */ { /* f */ }/* after */ }.prototype, "f").set;
let g = Object.getOwnPropertyDescriptor(class { /* before */set /* a */ [ /* b */ "g" /* c */ ] /* d */ ( /* e */ a /* f */ ) /* g */ { /* h */ }/* after */ }.prototype, "g").set;

assert.sameValue(f.toString(), "set /* a */ f /* b */ ( /* c */ a /* d */ ) /* e */ { /* f */ }");
assert.sameValue(g.toString(), "set /* a */ [ /* b */ \"g\" /* c */ ] /* d */ ( /* e */ a /* f */ ) /* g */ { /* h */ }");

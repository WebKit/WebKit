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
var BUGNUMBER = 924672;
var summary = 'Method Definitions - Generators'

print(BUGNUMBER + ": " + summary);

// Function definitions.
function syntaxError (script) {
    try {
        Function(script);
    } catch (e) {
        if (e instanceof SyntaxError) {
            return;
        }
    }
    throw new Error('Expected syntax error: ' + script);
}


// Tests begin.

syntaxError("{*a(){}}");
syntaxError("b = {*(){}");
syntaxError("b = {*{}");
syntaxError("b = {*){}");
syntaxError("b = {*({}");
syntaxError("b = {*(){");
syntaxError("b = {*()}");
syntaxError("b = {*a(");
syntaxError("b = {*a)");
syntaxError("b = {*a(}");
syntaxError("b = {*a)}");
syntaxError("b = {*a()");
syntaxError("b = {*a()}");
syntaxError("b = {*a(){}");
syntaxError("b = {*a){}");
syntaxError("b = {*a}}");
syntaxError("b = {*a{}}");
syntaxError("b = {*a({}}");
syntaxError("b = {*a@(){}}");
syntaxError("b = {*@(){}}");
syntaxError("b = {*get a(){}}");
syntaxError("b = {get *a(){}}");
syntaxError("b = {get a*(){}}");
syntaxError("b = {*set a(c){}}");
syntaxError("b = {set *a(c){}}");
syntaxError("b = {set a*(c){}}");
syntaxError("b = {*a : 1}");
syntaxError("b = {a* : 1}");
syntaxError("b = {a :* 1}");
syntaxError("b = {a*(){}}");

// Generator methods.
b = { * g() {
    var a = { [yield 1]: 2, [yield 2]: 3};
    return a;
} }
var it = b.g();
var next = it.next();
assert.sameValue(next.done, false);
assert.sameValue(next.value, 1);
next = it.next("hello");
assert.sameValue(next.done, false);
assert.sameValue(next.value, 2);
next = it.next("world");
assert.sameValue(next.done, true);
assert.sameValue(next.value.hello, 2);
assert.sameValue(next.value.world, 3);

// prototype property
assert.sameValue(b.g.hasOwnProperty("prototype"), true);

// Strict mode
a = {*b(c){"use strict";yield c;}};
assert.sameValue(a.b(1).next().value, 1);
a = {*["b"](c){"use strict";return c;}};
assert.sameValue(a.b(1).next().value, 1);

// Generators should not have [[Construct]]
a = {*g() { yield 1; }}
assertThrowsInstanceOf(() => { new a.g }, TypeError);


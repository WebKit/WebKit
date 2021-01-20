/***
 * There was a bug on 32-bit builds where === with objects would not check the tag
 * when determining equality via pointer comparison.
 */

"use strict";

function Foo() {}

function checkStrictEq(a, b) {
    return a === b;
}
noInline(checkStrictEq);

function checkStrictEqOther(a, b) {
    return a === b;
}
noInline(checkStrictEqOther);

var foo = new Foo();
var address = addressOf(foo);

if (address === undefined)
    throw "Error: address should not be undefined";

if (foo === address || address === foo)
    throw "Error: an address should not be equal to it's object";

for (var i = 0; i < 10000000; i++) {
    if (checkStrictEq(foo, address))
        throw "Error: an address should not be equal to it's object";
    if (checkStrictEqOther(address,foo))
        throw "Error: an address should not be equal to it's object";
}

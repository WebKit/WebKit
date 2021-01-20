//@ skip if $architecture == "x86"

var createBuiltin = $vm.createBuiltin;
var iterationCount = 100000;

// This is pretty bad but I need a private name.
var putFuncToPrivateName = createBuiltin(`(function (func) { @generatorThis = func })`)

function createTailCallForwardingFuncWith(body, thisValue) {
    return createBuiltin(`(function (a) {
        "use strict";

        ${body}

        return @tailCallForwardArguments(@generatorThis, ${thisValue});
    })`);
}

let bodyText = `
for (let i = 0; i < 100; i++) {
    if (a + i === 100)
        return a;
}
`;

putFuncToPrivateName(function() { return arguments; });
var foo6 = createTailCallForwardingFuncWith(bodyText, "{ thisValue: this, otherValue: 'hello'} ");
function baz6() {
    "use strict"
    return foo6.apply(this, arguments);
}
noInline(baz6);

function arrayEq(a, b) {
    if (a.length !== b.length)
        throw new Error();
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error();
    }
}
let args = ["a", {}, [], Symbol(), 1, 1.234, undefined, null];
for (let i = 0; i < iterationCount; i++) {
    let result = baz6.apply(undefined, args);
    arrayEq(result, args);
}

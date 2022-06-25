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

putFuncToPrivateName(function () { "use strict"; return this;  });
let bodyText = `
for (let i = 0; i < 100; i++) {
    if (a + i === 100)
        return a;
}
`;

var testFunc = function () { "use strict"; return this;  }
noInline(testFunc);
putFuncToPrivateName(testFunc);

var foo5 = createTailCallForwardingFuncWith(bodyText, "{ thisValue: this, otherValue: 'hello'} ");
var fooNI5 = createTailCallForwardingFuncWith(bodyText, "{ thisValue: this, otherValue: 'hello'} ");
noInline(fooNI5);
function baz5() {
    return foo5.call(true, 0);
}
noInline(baz5);

for (let i = 0; i < iterationCount; i++) {
    let result = foo5.call(true, 0);
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = baz5.call();
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = fooNI5.call(true, 0);
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = fooNI5.call(true, 1);
    if (result !== 1)
        throw new Error(i);
    result = fooNI5.call(true, "");
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
}

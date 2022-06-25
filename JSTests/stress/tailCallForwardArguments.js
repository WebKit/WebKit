//@ skip if $architecture == "x86"

var createBuiltin = $vm.createBuiltin;
var iterationCount = 100000;

// This is pretty bad but I need a private name.
var putFuncToPrivateName = createBuiltin(`(function (func) { @generatorThis = func })`)
putFuncToPrivateName(function (a,b) { return b; })

function createTailCallForwardingFuncWith(body, thisValue) {
    return createBuiltin(`(function (a) {
        "use strict";

        ${body}

        return @tailCallForwardArguments(@generatorThis, ${thisValue});
    })`);
}

var foo = createTailCallForwardingFuncWith("", "@undefined");

function baz() {
    return foo.call(true, 7);
}
noInline(baz);



var fooNoInline = createTailCallForwardingFuncWith("", "@undefined");
noInline(foo);

for (let i = 0; i < iterationCount; i++) {
    if (baz.call() !== undefined)
        throw new Error(i);
    if (fooNoInline.call(undefined, 3) !== undefined)
        throw new Error(i);
}

putFuncToPrivateName(function () { "use strict"; return { thisValue: this, argumentsValue: arguments};  });
var foo2 = createTailCallForwardingFuncWith("", "this");
var fooNI2 = createTailCallForwardingFuncWith("", "this");
noInline(fooNI2);

function baz2() {
    return foo2.call(true, 7);
}
noInline(baz2);

for (let i = 0; i < iterationCount; i++) {
    let result = foo2.call(true, 7);
    if (result.thisValue !== true || result.argumentsValue.length !== 1 || result.argumentsValue[0] !== 7)
        throw new Error(i);
    result = baz2.call();
    if (result.thisValue !== true || result.argumentsValue.length !== 1 || result.argumentsValue[0] !== 7)
        throw new Error(i);
    result = fooNI2.call(true, 7);
    if (result.thisValue !== true || result.argumentsValue.length !== 1 || result.argumentsValue[0] !== 7)
        throw new Error(i);
}

putFuncToPrivateName(function () { "use strict"; return this;  });
var foo3 = createTailCallForwardingFuncWith("", "{ thisValue: this, otherValue: 'hello'} ");
var fooNI3 = createTailCallForwardingFuncWith("", "{ thisValue: this, otherValue: 'hello'} ");
noInline(fooNI3);
function baz3() {
    return foo3.call(true, 7);
}
noInline(baz3);

for (let i = 0; i < iterationCount; i++) {
    let result = foo3.call(true, 7);
    if (result.thisValue !== true)
        throw new Error(i);
    result = baz3.call();
    if (result.thisValue !== true)
        throw new Error(i);
    result = fooNI3.call(true, 7);
    if (result.thisValue !== true)
        throw new Error(i);
}

//@ skip if $architecture == "x86"

var createBuiltin = $vm.createBuiltin;

// This is pretty bad but I need a private name.
var putFuncToPrivateName = createBuiltin(`(function (func) { @arrayIteratorIsDone = func })`)
putFuncToPrivateName(function (a,b) { return b; })

function createTailCallForwardingFuncWith(body, thisValue) {
    return createBuiltin(`(function (a) {
        "use strict";

        ${body}

        return @tailCallForwardArguments(@arrayIteratorIsDone, ${thisValue});
    })`);
}

var foo = createTailCallForwardingFuncWith("", "@undefined");

function baz() {
    return foo.call(true, 7);
}
noInline(baz);



var fooNoInline = createTailCallForwardingFuncWith("", "@undefined");
noInline(foo);

for (let i = 0; i < 100000; i++) {
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

for (let i = 0; i < 100000; i++) {
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

for (let i = 0; i < 100000; i++) {
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


putFuncToPrivateName(function () { "use strict"; return this;  });
let bodyText = `
for (let i = 0; i < 100; i++) {
    if (a + i === 100)
        return a;
}
`;
var foo4 = createTailCallForwardingFuncWith(bodyText, "{ thisValue: this, otherValue: 'hello'} ");
var fooNI4 = createTailCallForwardingFuncWith(bodyText, "{ thisValue: this, otherValue: 'hello'} ");
noInline(fooNI4);
function baz4() {
    return foo4.call(true, 0);
}
noInline(baz4);

for (let i = 0; i < 100000; i++) {
    let result = foo4.call(true, 0);
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = baz4.call();
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = fooNI4.call(true, 0);
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
    result = fooNI4.call(true, 1);
    if (result !== 1)
        throw new Error(i);
    result = fooNI4.call(true, "");
    if (result.thisValue !== true || result.otherValue !== "hello")
        throw new Error(i);
}

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

for (let i = 0; i < 100000; i++) {
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
for (let i = 0; i < 100000; i++) {
    let result = baz6.apply(undefined, args);
    arrayEq(result, args);
}

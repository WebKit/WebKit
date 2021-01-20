let expectedThis;

function applyHandler(target, thisValue) {
    if (thisValue !== expectedThis)
        throw new Error("got weird this value: " + thisValue);
}

let handler = {
    apply: applyHandler
};
let f = new Proxy(function() {}, handler);

function withScope(o) {
    with (o) {
        f();
    }
}

function lexicalScope() {
    let x;
    f();
}

// globalScope
f();

function strictEvalScope() {
    "use strict";
    eval("var x; f();");
}

let primitives = [undefined, null, true, 1.324, "test", Symbol("test"), BigInt(12), {}, []];

for (let primitive of primitives) {
    expectedThis = primitive;
    f.call(primitive);
}

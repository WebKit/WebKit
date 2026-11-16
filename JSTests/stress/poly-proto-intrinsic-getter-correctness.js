function assert(b) {
    if (!b)
        throw new Error("Bad!");
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 42;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

let x = new Uint32Array(10);
let p = x.__proto__.__proto__;
let obj = makePolyProtoObject();
obj.__proto__ = p;
x.__proto__ = obj;

function foo(x) {
    return x.byteLength;
}
noInline(foo);

for (let i = 0; i < 1000; ++i) {
    assert(foo(x) === 10 * 4);
};

obj.__proto__ = {};

assert(foo(x) === undefined);

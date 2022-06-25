function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
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

let global;

function performSet(o) {
    o.p = 20;
}

let start = Date.now();
for (let i = 0; i < 1000; ++i) {
    let obj = makePolyProtoObject();
    obj.__proto__ = null;
    performSet(obj);
    assert(Object.hasOwnProperty.call(obj, "p"));
    assert(obj.p === 20);

}

for (let i = 0; i < 1000; ++i) {
    let obj = makePolyProtoObject();
    obj.__proto__ = { set p(x) { global = x; } };
    performSet(obj);
    assert(!obj.hasOwnProperty("p"));
    assert(global === 20);
    global = null;
}

for (let i = 0; i < 1000; ++i) {
    let obj = makePolyProtoObject();
    performSet(obj);
    assert(obj.hasOwnProperty("p"));
    assert(obj.p === 20);
}
if (false)
    print(Date.now() - start);

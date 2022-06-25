function makePolyProtoInstanceWithNullPrototype() {
    function foo() {
        class C {
            constructor() { this.x = 20; }
        };
        C.prototype.y = 42;
        let result = new C;
        return result;
    }

    for (let i = 0; i < 5; ++i)
        foo();
    let result = foo();
    result.__proto__ = null;
    return result;
}

function assert(b) {
    if (!b)
        throw new Error("Bad asssertion")
}

let instances = [
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
    makePolyProtoInstanceWithNullPrototype(),
];

let p = undefined;
function validate(x) {
    assert(x.x === 20);
    assert(x.p === undefined);
}
noInline(validate);

let start = Date.now();
for (let i = 0; i < 100000; ++i) {
    for (let i = 0; i < instances.length; ++i)
        validate(instances[i]);
}
if (false)
    print(Date.now() - start);

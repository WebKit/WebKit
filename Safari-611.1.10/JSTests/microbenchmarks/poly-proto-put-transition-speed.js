//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() { this._field = 42; }
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

let items = [];
for (let i = 0; i < 25; ++i) {
    let item = makePolyProtoObject();
    item.__proto__ = null;
    items.push(item);
}

let start = Date.now();
for (let i = 0; i < 100000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        let obj = items[i];
        performSet(obj);
        assert(Object.hasOwnProperty.call(obj, "p"));
        assert(obj.p === 20);
        assert(obj._field === 42);
    }

}
if (false)
    print(Date.now() - start);

// This tests that intrinsics that are attached to self of an object correctly abort
// when the self value is changed.

(function body() {
    function foo(a) {
        return a.length;
    }
    noInline(foo);

    function bar(a) {
        return a.byteLength;
    }
    noInline(bar);

    function baz(a) {
        return a.byteOffset;
    }
    noInline(baz);

    let array = new Int32Array(10);

    let properties = ["length", "byteLength", "byteOffset"];
    properties.map(function(name) {
        let getter = Int32Array.prototype.__lookupGetter__(name);
        Object.defineProperty(array, name, { get: getter, configurable: true });
    });

    for (let i = 0; i < 100000; i++)
        foo(array);

    properties.map(function(name) {
        Object.defineProperty(array, name, { value: null });
    });

    if (foo(array) !== null)
        throw "length should have been null";

    if (bar(array) !== null)
        throw "byteLength should have been null";

    if (baz(array) !== null)
        throw "byteOffset should have been null";
})();

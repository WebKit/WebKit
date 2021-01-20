// This tests that TypedArray length and byteLength correctly dump code when the prototypes move.

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

    let array = new Int32Array(15);

    while(numberOfDFGCompiles(foo) < 1) {
        foo(array);
        bar(array);
        baz(array);
    }

    Object.setPrototypeOf(array, null);

    let passed = false;

    if (foo(array) !== undefined)
        throw "length should have become undefined when the prototype changed";
    if (bar(array) !== undefined)
        throw "byteLength should have become undefined when the prototype changed";
    if (baz(array) !== undefined)
        throw "byteOffset should have become undefined when the prototype changed";


})();

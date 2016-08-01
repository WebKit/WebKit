var A = class A { };
var B = class B extends A {
    constructor(beforeSuper, returnThis) {
        var f = () => returnThis ? (() => this ) : (()=>{});
        let af = f();
        if (beforeSuper) {
            let result = af();
            super();
        } else {
            super();
            let result = af();
        }
    }
};

var exception = null;
for (var i = 0; i < 10000; i++) {
    try {
        new B(true, true);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown was not a reference error";
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration";

    var a = new B(false, true);
    var b = new B(false, false);
    var c = new B(true, false);
}

// This test shouldn't crash.

class A { };

class B extends A {
    constructor(a, b) {
        var f = () => b ? this : {};
        if (a) {
            var val = f() == super();
        } else {
            super();
            var val = f();
        }
    }
};

for (var i=0; i < 10000; i++) {
    try {
        new B(true, true);
    } catch (e) {
    }
    var a = new B(false, true);
    var c = new B(true, false);
}

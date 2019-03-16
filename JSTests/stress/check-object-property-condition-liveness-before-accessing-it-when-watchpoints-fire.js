//@ requireOptions("--forceEagerCompilation=true")

// This test should not crash.

let a;

function bar() {
    a / 1;
    a = null;
}

function foo(s) {
    try {
        eval(s);
    } catch (e) {
        gc();
        bar();
        '' + e + 0;
    }
}

foo('zz');
foo('class A { y() {} }; a=new A; zz');
foo('class C { y() {} }; gc();');

class A {
    y() {}
}

A.prototype.z = 0

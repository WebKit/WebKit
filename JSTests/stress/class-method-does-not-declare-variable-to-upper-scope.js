function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// This is the minimized test case for the crash.
// https://bugs.webkit.org/show_bug.cgi?id=150115
(function () {
    eval("class A { static 1() { return 405 } };");
}());

(function () {
    class A {
        method() {
            shouldBe(typeof staticMethod, 'undefined');
        }

        static staticMethod() {
            shouldBe(typeof method, 'undefined');
        }
    }

    shouldBe(typeof method, 'undefined');
    shouldBe(typeof staticMethod, 'undefined');

    let a = new A();
    a.method();
    A.staticMethod();
}());

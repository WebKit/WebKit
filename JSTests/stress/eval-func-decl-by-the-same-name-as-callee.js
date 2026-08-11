function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

(function foo(a) {
    shouldBe(foo, undefined);

    if (true) {
        function foo(b) {}
    }

    shouldBe(foo.toString(), "function foo(b) {}");
})();

const bar__ = function bar(a) {
    shouldBe(bar, bar__);

    eval(`if (true) { function bar(b) {} }`);

    shouldBe(bar.toString(), "function bar(b) {}");
};

bar__();

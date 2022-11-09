function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let expected = 42;

(function x([x]) {

});

(function x([x]) {
    shouldBe(x, expected);
}([expected]));

(function x([x]) {
    shouldBe(x, expected);
})([expected]);

(function x(x) {

});

(function x(x) {
    shouldBe(x, expected);
}(expected));

(function x(x) {
    shouldBe(x, expected);
})(expected);

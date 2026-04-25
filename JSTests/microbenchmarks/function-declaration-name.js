function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    "use strict";
    function test(n, count) {
        if (n) {
            return (function (n, count) {
                return test(n, count);
            }(n - 1, count + 1));
        }
        return count;
    }
    shouldBe(test(1e7, 0), 1e7);
}());

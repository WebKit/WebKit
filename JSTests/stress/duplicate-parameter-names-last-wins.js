function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function direct(x, x) { return x; }
function captured(x, x) { return function() { return x; }; }
function withEval(x, x) { return eval("x"); }
function withArguments(x, x) { arguments; return x; }
function fullActivation(x, y, x) { return function() { return x + y; }; }
function mixed(a, b, b, b, b, b, b) { return function() { return a; }; }

for (var i = 0; i < 1e4; ++i) {
    shouldBe(direct(1, 2), 2);
    shouldBe(direct(1), undefined);
    shouldBe(captured(1, 2)(), 2);
    shouldBe(withEval(1, 2), 2);
    shouldBe(withArguments(1, 2), 2);
    shouldBe(fullActivation(1, 10, 2)(), 12);
    shouldBe(mixed("success")(), "success");
}

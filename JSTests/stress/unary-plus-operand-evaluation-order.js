function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}, expected: ${errorType.name}`);
    return error;
}

const operators = ["*", "/", "%", "-", "**", "+"];

for (const operator of operators) {
    const expected = eval(`6 ${operator} 3`);

    // The unary plus on the left operand converts it before the right operand is evaluated.
    {
        const log = [];
        const left = { valueOf() { log.push("left.valueOf"); return 6; } };
        const right = { valueOf() { log.push("right.valueOf"); return 3; } };
        const func = new Function("log", "left", "right", `return (+(log.push("left"), left)) ${operator} (log.push("right"), right);`);
        shouldBe(func(log, left, right), expected);
        shouldBe(log.join(), "left,left.valueOf,right,right.valueOf");
    }

    // The unary plus on the right operand converts it before the operator converts the left operand.
    {
        const log = [];
        const left = { valueOf() { log.push("left.valueOf"); return 6; } };
        const right = { valueOf() { log.push("right.valueOf"); return 3; } };
        const func = new Function("log", "left", "right", `return (log.push("left"), left) ${operator} +(log.push("right"), right);`);
        shouldBe(func(log, left, right), expected);
        shouldBe(log.join(), "left,right,right.valueOf,left.valueOf");
    }

    {
        const log = [];
        const left = { valueOf() { log.push("left.valueOf"); return 6; } };
        const right = { valueOf() { log.push("right.valueOf"); return 3; } };
        const func = new Function("log", "left", "right", `return (+(log.push("left"), left)) ${operator} +(log.push("right"), right);`);
        shouldBe(func(log, left, right), expected);
        shouldBe(log.join(), "left,left.valueOf,right,right.valueOf");
    }

    // The conversion of the left operand is observable by the right operand.
    {
        let right = 0;
        const left = { valueOf() { right = 3; return 6; } };
        shouldBe(eval(`(+left) ${operator} right`), expected);
    }

    // The right operand is not evaluated when the unary plus on the left operand throws.
    {
        let called = false;
        const left = { valueOf() { throw new RangeError("left"); } };
        const right = () => { called = true; throw new Error("right"); };
        const func = new Function("left", "right", `return (+left) ${operator} right();`);
        shouldBe(shouldThrow(() => func(left, right), RangeError).message, "left");
        shouldBe(called, false);
    }

    // Unary plus throws for BigInt while the operator itself accepts it.
    shouldBe(new Function("a", "b", `return a ${operator} b;`)(6n, 3n), BigInt(expected));
    shouldThrow(() => new Function("a", "b", `return (+a) ${operator} b;`)(6n, 3n), TypeError);
    shouldThrow(() => new Function("a", "b", `return a ${operator} +b;`)(6n, 3n), TypeError);
    shouldThrow(() => new Function("a", "b", `return (+a) ${operator} +b;`)(6n, 3n), TypeError);
    shouldThrow(() => new Function("a", `return (+a) ${operator} 3;`)({ valueOf() { return 6n; } }), TypeError);
    shouldThrow(new Function(`return (+6n) ${operator} 3n;`), TypeError);
    shouldThrow(new Function(`return 6n ${operator} +3n;`), TypeError);
    {
        let called = false;
        shouldThrow(() => new Function("a", "b", `return (+a) ${operator} b();`)(6n, () => { called = true; return 3n; }), TypeError);
        shouldBe(called, false);
    }

    // Unary plus on a number is not observable.
    shouldBe(new Function(`return (+6) ${operator} 3;`)(), expected);
    shouldBe(new Function(`return 6 ${operator} +3;`)(), expected);
    shouldBe(new Function(`return (+6) ${operator} +3;`)(), expected);
    shouldBe(new Function(`return (+6.0) ${operator} +3.0;`)(), expected);
    shouldBe(new Function(`return (+ +6) ${operator} + +3;`)(), expected);
    shouldBe(new Function("a", "b", `return (+a) ${operator} +b;`)(6, 3), expected);
    shouldBe(new Function("a", "b", `return (+a) ${operator} +b;`)("6", "3"), expected);
    shouldBe(new Function("a", "b", `return (+(a >>> 0)) ${operator} +(b >>> 0);`)(6, 3), expected);
    shouldBe(new Function("a", "b", `return (+(+a)) ${operator} +(+b);`)("6", "3"), expected);
}

// Unary plus on an operand which is already a number does not convert it again.
{
    let count = 0;
    const value = { valueOf() { ++count; return 7; } };
    shouldBe(new Function("x", "return +x * 1;")(value), 7);
    shouldBe(new Function("x", "return 1 * +x;")(value), 7);
    shouldBe(new Function("x", "return x * 1;")(value), 7);
    shouldBe(new Function("x", "return 1 * x;")(value), 7);
    shouldBe(new Function("x", "return +(+x);")(value), 7);
    shouldBe(new Function("x", "return + + +x;")(value), 7);
    shouldBe(new Function("x", "return +(+x * 1);")(value), 7);
    shouldBe(new Function("x", "return +(-(+x));")(value), -7);
    shouldBe(count, 8);

    shouldThrow(() => new Function("x", "return +x * 1;")(7n), TypeError);
    shouldThrow(() => new Function("x", "return 1 * +x;")(7n), TypeError);
    shouldThrow(() => new Function("x", "return x * 1;")(7n), TypeError);
    shouldThrow(() => new Function("x", "return 1 * x;")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(+x);")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(-x);")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(x - 1n);")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(x >> 1n);")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(x || 1);")(7n), TypeError);
    shouldThrow(() => new Function("x", "return +(x ?? 1);")(7n), TypeError);
}

shouldBe(Object.is(+(-0), -0), true);
shouldBe(Object.is(+(-0) * 1, -0), true);
shouldBe(Object.is(1 * +(-0), -0), true);
shouldBe(Object.is(+NaN, NaN), true);
shouldBe(+(-1 >>> 0), 4294967295);
shouldBe(+(-1 >>> 0) < (1 >>> 0), false);
shouldBe(+(1 >>> 0) < +(-1 >>> 0), true);
shouldBe(new Function("a", "b", "return +(a >>> 0) < (b >>> 0);")(-1, 1), false);
shouldBe(new Function("a", "b", "return (a >>> 0) < +(b >>> 0);")(1, -1), true);

function plusInSwitch(value) {
    switch (value) {
    case +1:
        return "one";
    case +2.5:
        return "two and a half";
    case -3:
        return "minus three";
    case + +4:
        return "four";
    }
    return "none";
}
shouldBe(plusInSwitch(1), "one");
shouldBe(plusInSwitch(2.5), "two and a half");
shouldBe(plusInSwitch(-3), "minus three");
shouldBe(plusInSwitch(4), "four");
shouldBe(plusInSwitch("1"), "none");
shouldBe(plusInSwitch(1n), "none");

shouldThrow(() => eval("+1 = 2"), SyntaxError);
shouldThrow(() => eval("+1++"), SyntaxError);
shouldThrow(() => eval("+1 ** 2"), SyntaxError);
shouldThrow(() => eval("+(+a) ** 2"), SyntaxError);
shouldBe((+2) ** 3, 8);

function multiply(left, right) { return +left * right(); }
noInline(multiply);
function divide(left, right) { return +left / right(); }
noInline(divide);
function remainder(left, right) { return +left % right(); }
noInline(remainder);
function subtract(left, right) { return +left - right(); }
noInline(subtract);
function power(left, right) { return (+left) ** right(); }
noInline(power);
function multiplyRight(left, right) { return left * +right; }
noInline(multiplyRight);
function subtractBoth(left, right) { return +left - +right; }
noInline(subtractBoth);

{
    const log = [];
    const left = { valueOf() { log.push("left.valueOf"); return 6; } };
    const right = { valueOf() { log.push("right.valueOf"); return 3; } };
    const callRight = () => { log.push("right"); return 3; };

    for (let i = 0; i < testLoopCount; ++i) {
        log.length = 0;
        shouldBe(multiply(left, callRight), 18);
        shouldBe(divide(left, callRight), 2);
        shouldBe(remainder(left, callRight), 0);
        shouldBe(subtract(left, callRight), 3);
        shouldBe(power(left, callRight), 216);
        shouldBe(log.join(), "left.valueOf,right,left.valueOf,right,left.valueOf,right,left.valueOf,right,left.valueOf,right");

        log.length = 0;
        shouldBe(multiplyRight(left, right), 18);
        shouldBe(log.join(), "right.valueOf,left.valueOf");

        log.length = 0;
        shouldBe(subtractBoth(left, right), 3);
        shouldBe(log.join(), "left.valueOf,right.valueOf");
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(multiply(6, () => 3), 18);
    shouldBe(multiplyRight(6, 3), 18);
    shouldBe(subtractBoth(6.5, "3"), 3.5);
}

shouldThrow(() => multiply(6n, () => 3n), TypeError);
shouldThrow(() => multiplyRight(6n, 3n), TypeError);
shouldThrow(() => subtractBoth(6n, 3n), TypeError);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function Pair(car, cdr) {
    this.car = car;
    this.cdr = cdr;
}

class Custom {
    static [Symbol.hasInstance](value) { return value === 42; }
}

function notInstance(term) {
    if (!(term instanceof Pair))
        return term;
    return term.car;
}
noInline(notInstance);

function isInstance(term) {
    if (term instanceof Pair)
        return term.cdr;
    return -1;
}
noInline(isInstance);

function resultUsedAfterBranch(term) {
    var result = term instanceof Pair;
    if (result)
        return [result, term.car];
    return [result, term];
}
noInline(resultUsedAfterBranch);

function custom(value, constructor) {
    if (value instanceof constructor)
        return "yes";
    return "no";
}
noInline(custom);

function loop(list) {
    var count = 0;
    while (list instanceof Pair) {
        count += list.car;
        list = list.cdr;
    }
    return count;
}
noInline(loop);

function customJtrue(value, constructor) {
    if (!(value instanceof constructor))
        return "no";
    return "yes";
}
noInline(customJtrue);

function argumentDst(a, b) {
    a = a instanceof Pair;
    if (a)
        return b;
    return -b;
}
noInline(argumentDst);

function longBody(term) {
    if (term instanceof Pair) {
        var x = term.car;
        x = (x * 3 + 1) | 0; x = (x * 5 + 2) | 0; x = (x * 7 + 3) | 0; x = (x * 11 + 4) | 0;
        x = (x * 3 + 5) | 0; x = (x * 5 + 6) | 0; x = (x * 7 + 7) | 0; x = (x * 11 + 8) | 0;
        x = (x * 3 + 9) | 0; x = (x * 5 + 10) | 0; x = (x * 7 + 11) | 0; x = (x * 11 + 12) | 0;
        x = (x * 3 + 13) | 0; x = (x * 5 + 14) | 0; x = (x * 7 + 15) | 0; x = (x * 11 + 16) | 0;
        x = (x * 3 + 17) | 0; x = (x * 5 + 18) | 0; x = (x * 7 + 19) | 0; x = (x * 11 + 20) | 0;
        return x;
    }
    return -1;
}
noInline(longBody);

function longBodyExpected(car) {
    var x = car;
    var muls = [3, 5, 7, 11];
    for (var j = 1; j <= 20; ++j)
        x = (x * muls[(j - 1) % 4] + j) | 0;
    return x;
}

function inlinedPredicate(term) {
    if (!(term instanceof Pair))
        return 0;
    return term.car + term.cdr;
}

function inlinedCaller(term) {
    return inlinedPredicate(term) + 1;
}
noInline(inlinedCaller);

var list = new Pair(1, new Pair(2, new Pair(3, null)));
for (var i = 0; i < testLoopCount; ++i) {
    var pair = new Pair(i, i + 1);
    shouldBe(notInstance(pair), i);
    shouldBe(notInstance("str"), "str");
    shouldBe(notInstance(i), i);
    shouldBe(notInstance({ car: 1 }).car, 1);
    shouldBe(isInstance(pair), i + 1);
    shouldBe(isInstance(null), -1);
    shouldBe(isInstance({ cdr: 2 }), -1);

    var r = resultUsedAfterBranch(pair);
    shouldBe(r[0], true);
    shouldBe(r[1], i);
    r = resultUsedAfterBranch(undefined);
    shouldBe(r[0], false);
    shouldBe(r[1], undefined);

    shouldBe(custom(pair, Pair), "yes");
    shouldBe(custom(42, Custom), "yes");
    shouldBe(custom(pair, Custom), "no");
    shouldBe(custom(1, Pair), "no");

    shouldBe(loop(list), 6);

    shouldBe(customJtrue(pair, Pair), "yes");
    shouldBe(customJtrue(42, Custom), "yes");
    shouldBe(customJtrue(pair, Custom), "no");
    shouldBe(customJtrue(1, Pair), "no");

    shouldBe(argumentDst(pair, i), i);
    shouldBe(argumentDst({ car: 1 }, i), -i);

    shouldBe(longBody(pair), longBodyExpected(i));
    shouldBe(longBody({ car: 1 }), -1);

    shouldBe(inlinedCaller(pair), 2 * i + 2);
    shouldBe(inlinedCaller("str"), 1);
}

var threw = false;
try {
    custom(1, 2);
} catch (e) {
    threw = e instanceof TypeError;
}
shouldBe(threw, true);

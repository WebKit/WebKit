function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function isSeparator(c) { return c == ' ' || c == '\n'; }
noInline(isSeparator);

function isSeparatorReversed(c) { return ' ' == c || '\n' == c; }
noInline(isSeparatorReversed);

function looseEqual(a, b) { return a == b; }
noInline(looseEqual);

function makeRope(head, tail) { return head + tail; }
noInline(makeRope);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(isSeparator(' '), true);
    shouldBe(isSeparator('\n'), true);
    shouldBe(isSeparator('x'), false);
    shouldBe(isSeparator(undefined), false);
    shouldBe(isSeparator(null), false);
    shouldBe(isSeparatorReversed(' '), true);
    shouldBe(isSeparatorReversed(undefined), false);
    shouldBe(isSeparatorReversed('y'), false);
    shouldBe(looseEqual(makeRope('a', 'bbbbbbbbbb'), 'abbbbbbbbbb'), true);
    shouldBe(looseEqual(makeRope('a', 'bbbbbbbbbb'), 'abbbbbbbbbc'), false);
    shouldBe(looseEqual(undefined, 'abbbbbbbbbb'), false);
    shouldBe(looseEqual(null, 'x'), false);
}

shouldBe(isSeparator(32), false);
shouldBe(isSeparatorReversed({ toString() { return ' '; } }), true);
shouldBe(looseEqual(1, '1'), true);
shouldBe(looseEqual('1', 1), true);
shouldBe(looseEqual({ toString() { return 'x'; } }, 'x'), true);
shouldBe(looseEqual(true, '1'), true);

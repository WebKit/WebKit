function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function collect(iterable) { let seen = []; for (let x of iterable) seen.push(x); return seen.join("|"); }
function breakAt(iterable, count) { let seen = []; for (let x of iterable) { seen.push(x); if (seen.length === count) break; } return seen.join("|"); }
function firstTwo(iterable) { let [a, b] = iterable; return a + "|" + b; }
noInline(collect); noInline(breakAt); noInline(firstTwo);

// One loop that sees several kinds of iterables.
let iterables = [
    ["abc", ["a", "b", "c"]],
    [[1, 2, 3], [1, 2, 3]],
    ["\u{1F600}x", ["\u{1F600}", "x"]],
    [new Set([1, 2]), [1, 2]],
    [new String("xy"), ["x", "y"]],
    [{ *[Symbol.iterator]() { yield 1; yield 2; } }, [1, 2]],
];
for (let i = 0; i < testLoopCount; i++) {
    let [iterable, expected] = iterables[i % iterables.length];
    shouldBe(collect(iterable), expected.join("|"));
    shouldBe(breakAt(iterable, 1), expected.slice(0, 1).join("|"));
    shouldBe(firstTwo(iterable), expected[0] + "|" + expected[1]);
}

// A close that has only seen Arrays meets a String, and the other way around.
function breakAtTwo(iterable) { let seen = []; for (let x of iterable) { seen.push(x); if (seen.length === 2) break; } return seen.join("|"); }
function breakAtTwoAgain(iterable) { let seen = []; for (let x of iterable) { seen.push(x); if (seen.length === 2) break; } return seen.join("|"); }
noInline(breakAtTwo); noInline(breakAtTwoAgain);
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(breakAtTwo([1, 2, 3]), "1|2");
    shouldBe(breakAtTwoAgain("xyz"), "x|y");
}
for (let i = 0; i < 10; i++) {
    shouldBe(breakAtTwo("xyz"), "x|y");
    shouldBe(breakAtTwoAgain([1, 2, 3]), "1|2");
}

// Not iterable after all.
function shouldThrowTypeError(f) {
    try {
        f();
    } catch (error) {
        shouldBe(error instanceof TypeError, true);
        return;
    }
    throw new Error("did not throw");
}
shouldThrowTypeError(() => collect(5));
shouldThrowTypeError(() => firstTwo(null));
shouldBe(collect("ab"), "a|b");

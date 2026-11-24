//@ requireOptions("--maxRegExpStackSize=65536")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowRangeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error(`expected RangeError but got ${error}`);
}

const string = "a".repeat(2000) + "c";
const regExp = new RegExp("(?:(?!(?:a|b)*d)a)*c" + "(x)?".repeat(1600));

if (regExp.test(string)) {
    shouldBe(RegExp.lastMatch, string);
    for (const name of ["lastParen", "$1"])
        shouldThrowRangeError(() => RegExp[name]);
    shouldBe(RegExp.leftContext, "");
    shouldBe(RegExp.rightContext, "");
    shouldThrowRangeError(() => { RegExp.input = "x"; });
    shouldBe(RegExp.input, string);
}

shouldBe(/b(c)d/.test("abcde"), true);
shouldBe(RegExp.lastMatch, "bcd");
shouldBe(RegExp.lastParen, "c");
shouldBe(RegExp.$1, "c");
shouldBe(RegExp.leftContext, "a");
shouldBe(RegExp.rightContext, "e");
shouldBe(RegExp.input, "abcde");

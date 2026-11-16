//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

let re = /x/g;

let cases = [
    ["aaxbbxcc", 2, "aaxbb", "cc"],
    ["aaxbbx\0\0", 2, "aaxbb", "\0\0"],
    ["aaxbbあxcc", 2, "aaxbbあ", "cc"],
    [("_".repeat(64) + "aaxbb".repeat(10) + "xcc" + "_".repeat(64)).substring(64, 117), 11, "aaxbb".repeat(10), "cc"],
];

let index = 0;
let expected = null;

function step() {
    if (expected) {
        let [leftContext, rightContext] = expected;
        shouldBe(RegExp.$1, "");
        shouldBe(RegExp.lastParen, "");
        shouldBe(RegExp.lastMatch, "x");
        shouldBe(RegExp.leftContext, leftContext);
        shouldBe(RegExp.rightContext, rightContext);
    }
    if (index === cases.length)
        return;
    let [input, count, leftContext, rightContext] = cases[index++];
    re.test("zzx");
    shouldBe(input.match(re).length, count);
    expected = [leftContext, rightContext];
    $vm.deleteAllCodeWhenIdle();
    setTimeout(step, 0);
}

step();

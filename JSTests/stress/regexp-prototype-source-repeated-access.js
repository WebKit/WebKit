//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

// Every read of "source" has to return the same thing, no matter what the previous reads found out about the pattern.
function check(regExp, expectedSource, expectedFlags = "") {
    for (let i = 0; i < 3; ++i) {
        shouldBe(regExp.source, expectedSource);
        shouldBe(regExp.toString(), `/${expectedSource}/${expectedFlags}`);
        shouldBe(regExp.flags, expectedFlags);
    }
}
noInline(check);

const long = "(?:[a-z0-9]{1,4}:){6}|\\/x[/]".repeat(4);
const long16Bit = "(?:[あ-ん]{1,4}:){6}|\\/x[/]".repeat(4);

// [pattern, source]
const cases = [
    // Nothing to escape.
    ["abc", "abc"],
    ["a", "a"],
    ["(?:)", "(?:)"],
    ["a\\/b", "a\\/b"],
    ["\\\\\\/", "\\\\\\/"],
    ["[/]", "[/]"],
    ["[\\]/]", "[\\]/]"],
    ["[[/]", "[[/]"],
    ["\\n\\r\\u2028\\u2029", "\\n\\r\\u2028\\u2029"],
    ["é+", "é+"],
    ["あ+[/]\\/", "あ+[/]\\/"],
    [long, long],
    [long16Bit, long16Bit],

    // Empty.
    ["", "(?:)"],

    // A slash.
    ["/", "\\/"],
    ["a/b", "a\\/b"],
    ["\\\\/", "\\\\\\/"],
    ["[/]/", "[/]\\/"],
    ["\\[/]", "\\[\\/]"],
    ["[\\]/]/", "[\\]/]\\/"],
    ["あ/", "あ\\/"],
    [long + "/", long + "\\/"],
    [long16Bit + "/", long16Bit + "\\/"],

    // A line terminator.
    ["\n", "\\n"],
    ["a\rb", "a\\rb"],
    ["\\\n", "\\n"],
    ["[\n]", "[\\n]"],
    ["\u2028", "\\u2028"],
    ["\\\u2029", "\\u2029"],
    ["あ\n", "あ\\n"],
    [long + "\n", long + "\\n"],
    [long16Bit + "\u2028", long16Bit + "\\u2028"],
];

for (let i = 0; i < testLoopCount / 5000; ++i) {
    for (const [pattern, source] of cases) {
        check(new RegExp(pattern), source);
        check(new RegExp(pattern, "gi"), source, "gi");

        // The source of one regular expression is the pattern of another.
        check(new RegExp(new RegExp(pattern).source), source);
        check(new RegExp(new RegExp(pattern), "y"), source, "y");
    }

    // Literals share the RegExp with the constructor.
    check(/a\/b/, "a\\/b");
    check(/[/]/, "[/]");
    check(/é+/, "é+");
}

// RegExp.prototype.compile() replaces the pattern.
{
    const regExp = new RegExp("initial");
    for (const [pattern, source] of cases) {
        regExp.compile(pattern);
        check(regExp, source);
        regExp.compile(pattern, "g");
        check(regExp, source, "g");
    }
}

// The returned string keeps its contents after the RegExp is collected.
{
    const sources = [];
    for (let i = 0; i < 100; ++i) {
        const regExp = new RegExp(`plain${i}[/]`);
        shouldBe(regExp.source, `plain${i}[/]`);
        sources.push(regExp.source);
    }
    fullGC();
    for (let i = 0; i < 100; ++i)
        shouldBe(sources[i], `plain${i}[/]`);
}

shouldBe(RegExp.prototype.source, "(?:)");

// Matching, and throwing away the compiled code, do not change the source.
// deleteAllCodeWhenIdle() does its work after the current turn, so the checks run in the next one.
{
    const plain = new RegExp("a[/]b+");
    const escaped = new RegExp("a/b+");
    let remaining = 4;
    function step() {
        check(plain, "a[/]b+");
        check(escaped, "a\\/b+");
        shouldBe(plain.exec("xa/bbx")[0], "a/bb");
        shouldBe(escaped.exec("xa/bbx")[0], "a/bb");
        shouldBe(plain.test("\u3042a/b"), true);
        shouldBe(escaped.test("\u3042a/b"), true);
        if (!--remaining)
            return;
        fullGC();
        $vm.deleteAllCodeWhenIdle();
        setTimeout(step, 0);
    }
    step();
}

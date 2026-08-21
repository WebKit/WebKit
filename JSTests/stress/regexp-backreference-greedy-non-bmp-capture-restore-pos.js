//@ runDefault("--useRegExpJIT=false")

function shouldBe(actual, expected)
{
    actual = JSON.stringify(actual);
    expected = JSON.stringify(expected);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

// E is a non-BMP code point, so a back reference to a capture containing it
// reads the input as a surrogate pair. When that read fails mid-way through a
// greedy back reference iteration, the input position must be restored.
var E = "\u{1F601}";

shouldBe(new RegExp("(" + E + ")(\\1*)?", "u").exec(E + "\n" + E), [E, E, undefined]);
shouldBe(new RegExp("(" + E + ")\\1*$", "u").exec(E + "ab"), null);
shouldBe(new RegExp("(" + E + ")\\1*(a)", "u").exec(E + "xya"), null);
shouldBe(new RegExp("(" + E + ")\\1+$", "u").exec(E + E + "ab"), null);
shouldBe(new RegExp("(a" + E + ")\\1*X", "u").exec("a" + E + "abcX"), null);
shouldBe(new RegExp("(?<n>" + E + ")\\k<n>*$", "u").exec(E + "zz"), null);
shouldBe(new RegExp("(" + E + ")\\1*$", "v").exec(E + "ab"), null);

shouldBe(new RegExp("(" + E + ")\\1*$", "u").exec(E + E + E), [E + E + E, E]);
shouldBe(new RegExp("(" + E + ")\\1*(a)", "u").exec(E + E + "a"), [E + E + "a", E, "a"]);

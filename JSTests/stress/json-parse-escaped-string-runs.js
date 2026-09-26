function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

// Escaped strings with long unescaped runs between the escapes.
{
    let run = "abcdefghijklmnopqrstuvwxyz0123456789".repeat(3);
    let value = "";
    for (let i = 0; i < 50; ++i)
        value += run.slice(0, i) + ["\n", "\"", "\\", "\t", "/", "\u0001", "\u00e9", "\u3042"][i % 8];
    let json = JSON.stringify({ value, list: [value, value] });
    let r = JSON.parse(json);
    shouldBe(r.value, value, "strict value");
    shouldBe(r.list[0], value, "strict list 0");
    shouldBe(r.list[1], value, "strict list 1");

    let e = eval("(" + json + ")");
    shouldBe(e.value, value, "eval value");
    shouldBe(e.list[1], value, "eval list 1");
}

// A run that reaches a raw control character right after an escape is still an error.
{
    let threw = false;
    try {
        JSON.parse('["' + "a".repeat(40) + '\\n' + "b".repeat(40) + '\u0001"]');
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true, "control character after escape");
}

// Unterminated escaped string.
{
    let threw = false;
    try {
        JSON.parse('["' + "a".repeat(40) + '\\n' + "b".repeat(40));
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true, "unterminated escaped string");
}

// Sloppy-mode single-quoted strings with escapes, where a double quote is ordinary text.
{
    let r = eval("({ a: ['" + "x".repeat(40) + "\\n\"" + "y".repeat(40) + "\\'" + "z".repeat(20) + "'] })");
    shouldBe(r.a[0], "x".repeat(40) + "\n\"" + "y".repeat(40) + "'" + "z".repeat(20), "sloppy single quote");
}

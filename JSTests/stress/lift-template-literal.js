function dump(callSite)
{
    return JSON.stringify({ cooked: callSite, raw: callSite.raw });
}

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(dump`\newcommand{\fun}{\textbf{Fun!}}`, `{"cooked":["\\newcommand{\\fun}{\\textbf{Fun!}}"],"raw":["\\\\newcommand{\\\\fun}{\\\\textbf{Fun!}}"]}`);
shouldBe(dump`\newcommand{\unicode}{\textbf{Unicode!}}`, `{"cooked":[null],"raw":["\\\\newcommand{\\\\unicode}{\\\\textbf{Unicode!}}"]}`);
shouldBe(dump`\newcommand{\xerxes}{\textbf{King!}}`, `{"cooked":[null],"raw":["\\\\newcommand{\\\\xerxes}{\\\\textbf{King!}}"]}`);
shouldBe(dump`Breve over the h goes \u{h}ere`, `{"cooked":[null],"raw":["Breve over the h goes \\\\u{h}ere"]}`);

function testTag(expected) {
    return function tag(callSite) {
        shouldBe(callSite.length, expected.cooked.length);
        shouldBe(callSite.raw.length, expected.raw.length);
        expected.cooked.forEach((value, index) => shouldBe(callSite[index], value));
        expected.raw.forEach((value, index) => shouldBe(callSite.raw[index], value));
    }
}

testTag({
    cooked: [ undefined ],
    raw: [ "\\unicode and \\u{55}" ],
})`\unicode and \u{55}`;

testTag({
    cooked: [ undefined, "test" ],
    raw: [ "\\unicode and \\u{55}", "test" ],
})`\unicode and \u{55}${42}test`;

testTag({
    cooked: [ undefined, undefined, "Cocoa" ],
    raw: [ "\\unicode and \\u{55}", "\\uhello", "Cocoa" ],
})`\unicode and \u{55}${42}\uhello${42}Cocoa`;

testTag({
    cooked: [ "Cocoa", undefined, undefined, "Cocoa" ],
    raw: [ "Cocoa", "\\unicode and \\u{55}", "\\uhello", "Cocoa" ],
})`Cocoa${42}\unicode and \u{55}${42}\uhello${42}Cocoa`;

testTag({
    cooked: [ "Cocoa", undefined, undefined, "Cocoa" ],
    raw: [ "Cocoa", "\\unicode and \\u{55}", "\\uhello", "Cocoa" ],
})`Cocoa${42}\unicode and \u{55}${42}\uhello${42}Cocoa`;

testTag({
    cooked: [ undefined, undefined, undefined ],
    raw: [ "\\00", "\\01", "\\1" ]
})`\00${42}\01${42}\1`;

testTag({
    cooked: [ undefined, undefined ],
    raw: [ "\\xo", "\\x0o" ]
})`\xo${42}\x0o`;

testTag({
    cooked: [ undefined, undefined, undefined, undefined ],
    raw: [ "\\uo", "\\u0o", "\\u00o", "\\u000o" ]
})`\uo${42}\u0o${42}\u00o${42}\u000o`;

testTag({
    cooked: [ undefined, undefined, undefined ],
    raw: [ "\\u{o", "\\u{0o", "\\u{110000o" ]
})`\u{o${42}\u{0o${42}\u{110000o`;

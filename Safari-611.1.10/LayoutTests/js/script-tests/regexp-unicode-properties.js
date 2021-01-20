description(
'Test for of RegExp Unicode properties'
);

let src1 = ""
for (let c = 1; c <= 0x7f; c++)
    src1 += String.fromCharCode(c);
let re1 = new RegExp("\\p{ASCII}*", "u")
var matchResult1 = src1.match(re1);
shouldBe('matchResult1[0]', 'src1');

let src2 = "p{Any-Old-Property}";
let re2 = /\p{Any-Old-Property}/;
shouldBeTrue('re2.test(src2)');

shouldThrow('/\\p{Any-Old-Property}/u.test(src2)', '"SyntaxError: Invalid regular expression: invalid property expression"');
shouldThrow('/\\p{Script=Hebrew/u.test("Testing")', '"SyntaxError: Invalid regular expression: invalid property expression"');
shouldThrow('/\\p{Hebrew/u.test("Testing")', '"SyntaxError: Invalid regular expression: invalid property expression"');
shouldThrow('/\\p{Hebrew}/u.test("Testing")', '"SyntaxError: Invalid regular expression: invalid property expression"');
shouldThrow('/\\p{Letter/u.test("Testing")', '"SyntaxError: Invalid regular expression: invalid property expression"');
shouldThrow('/\\p{Cc/u.test("Testing")', '"SyntaxError: Invalid regular expression: invalid property expression"');

let src3 = "DeadBeef4A11";
let re3a = /\p{ASCII_Hex_Digit}*/u;
shouldBe('src3.match(re3a)[0]', 'src3');
let re3b = /\p{AHex}*/u;
shouldBe('src3.match(re3b)[0]', 'src3');

let src4 = "The Rain In Spain Falls Mainly on THE Plains.";
let re4a = /\p{General_Category=Uppercase_Letter}/ug;
let exp4 = ["T", "R", "I", "S", "F", "M", "T", "H", "E", "P"];
shouldBe('src4.match(re4a)', 'exp4');
let re4c = /\p{General_Category=Lu}/ug;
shouldBe('src4.match(re4c)', 'exp4');
let re4d = /\p{gc=Lu}/ug;
shouldBe('src4.match(re4d)', 'exp4');
let re4e = /\p{Uppercase_Letter}/ug;
shouldBe('src4.match(re4e)', 'exp4');
let re4f = /\p{Lu}/ug;
shouldBe('src4.match(re4f)', 'exp4');

let src5 = "SOME LOWER CASE: abdudtrj\u017f\u00ba\u2147\ua739\u{118c4}";
let re5a = /\p{Lowercase}+/u;
shouldBe('src5.match(re5a)[0]', '"abdudtrj\u017f\u00ba\u2147\ua739\u{118c4}"');
let re5b = /\p{Lowercase_Letter}+/u;
shouldBe('src5.match(re5b)[0]', '"abdudtrj\u017f"');
let re5c = /\P{Lowercase_Letter}+/u;
shouldBe('src5.match(re5c)[0]', '"SOME LOWER CASE: "');

let src6 = "The Greek word \u03bb\u03cc\u03b3\u03bf\u03c2 means \"reason\".";
let re6a = /\p{Script_Extensions=Grek}+/u;
shouldBe('re6a.exec(src6)[0]', '"\u03bb\u03cc\u03b3\u03bf\u03c2"');
let re6b = /\P{Script_Extensions=Grek}+/u;
shouldBe('re6b.exec(src6)[0]', '"The Greek word "');
let re6c = /[ \p{Script=Latin}\p{Script_Extensions=Grek}]+/u;
shouldBe('re6c.exec(src6)[0]', '"The Greek word \u03bb\u03cc\u03b3\u03bf\u03c2 means "');

let src7a = "2 + 2 = 4";
let re7a = /\p{Math}/u;
shouldBe('re7a.exec(src7a)[0]', '"+"');
let re7b = /[\p{Math}]+/u;
shouldBe('re7b.exec(src7a)[0]', '"+"');
let re7c = /[\p{Number}\p{Math}]+/u;
shouldBe('re7c.exec(src7a)[0]', '"2"');
let re7d = /[\p{Number}\p{Math}\p{space}]+/u;
shouldBe('re7d.exec(src7a)[0]', '"2 + 2 = 4"');
let src7b = "2 ^ .5 = 1.4142135";
shouldBe('re7d.exec(src7b)[0]', '"2 ^ "');
let re7e = /[\p{Number}\p{Math}\p{space}\.]+/u;
shouldBe('re7e.exec(src7b)[0]', '"2 ^ .5 = 1.4142135"');
let src7c = "2 \u00f7 3 = ~0.666666";
shouldBe('re7e.exec(src7c)[0]', '"2 \u00f7 3 = ~0.666666"');
let re7f = /[\p{N}\{No}\p{Math}\p{space}\.]+/u;
let src7d = "5\u00b3 + 3\u00b2 = 134";
shouldBe('re7f.exec(src7d)[0]', '"5\u00b3 + 3\u00b2 = 134"');
let re7g = /[\p{Math}]+/ug;
shouldBe('src7d.match(re7g)', '["+", "="]');

let src8 = "Hello is \u0bb5\u0ba3\u0b95\u0bcd\u0b95\u0bae\u0bcd in Tamil";
let re8a = /[\p{Script_Extensions=Taml}]+/u;
shouldBe('re8a.exec(src8)[0]', '"\u0bb5\u0ba3\u0b95\u0bcd\u0b95\u0bae\u0bcd"');
let re8b = /[\p{Script_Extensions=Latn}\p{Script_Extensions=Taml} ]+/u;
shouldBe('re8b.exec(src8)[0]', '"Hello is \u0bb5\u0ba3\u0b95\u0bcd\u0b95\u0bae\u0bcd in Tamil"');

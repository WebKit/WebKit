description(

"This test checks that BOM is stripped from the source, but other Cf characters are not, despite \
what ECMA-262 says, see &lt;https://bugs.webkit.org/show_bug.cgi?id=4931>."

);

// U+FEFF ZERO WIDTH NO-BREAK SPACE (BOM)
// U+200F RIGHT-TO-LEFT MARK
// U+200E LEFT-TO-RIGHT MARK
// U+00AD SOFT HYPHEN
// U+2062 INVISIBLE TIMES
// U+200D ZERO WIDTH JOINER
// U+200C ZERO WIDTH NON-JOINER
// U+200B ZERO WIDTH SPACE
var testString = "﻿‏‎­⁢‍‌​";
shouldBe('escape(testString)',"'%u200F%u200E%AD%u2062%u200D%u200C%u200B'");

var testString2 = eval('"\uFEFF\u200F\u200E\u00AD\u2062\u200D\u200C\u200B"');
shouldBe('escape(testString2)',"'%u200F%u200E%AD%u2062%u200D%u200C%u200B'");

// A BOM is inside "shouldBe".
shoul﻿dBe("1", "1");

shouldThrow('var ZWJ_I‍nside;');

successfullyParsed = true;

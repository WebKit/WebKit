// This test that . followed by fixed character terms works with non-BMP characters

if (!/^.-clef/u.test("\u{1D123}-clef"))
    throw "Should have matched string with leading non-BMP with BOL anchored . in RE";

if (!/c.lef/u.test("c\u{1C345}lef"))
    throw "Should have matched string with non-BMP with . in RE";




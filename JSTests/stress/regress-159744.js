// Regression test for 159744.  This test should not crash or throw an exception.

function testRegExp(pattern, flags, string, result)
{
    let r = new RegExp(pattern, flags);
    if (r.exec(string) !== result)
        throw("Expected " + r + "exec(\"" + string + "\") to return " + result + ".");
}

testRegExp("((?=$))??(?:\\1){34359738368,}", "gm", "abc\nabc\nabc\nabc\n", null);
testRegExp("(\\w+)(?:\\s(\\1)){1100000000,}", "i", "word Word WORD WoRd", null);
testRegExp("\\d{4,}.{1073741825}", "", "1234567\u1234", null);
testRegExp("(?:abcd){2148473648,}", "", "abcdabcdabcd", null);
testRegExp("(?:abcd){2148473648,}", "y", "abcdabcdabcd", null);
testRegExp("(ab){1073741825,}(xy){1073741825,}", "", "abxyabxy", null);

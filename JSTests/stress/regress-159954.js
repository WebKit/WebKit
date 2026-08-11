// Regression test for 159954.  This test should not crash.

function testRegExpThrows(regexpExpression)
{
    try {
        let result = eval(regexpExpression);
        throw "Expected \"" + regexpExpression + "\" to throw and it didn't, got: " + JSON.stringify(result);
    } catch (e) {
        if (e != "SyntaxError: Invalid regular expression: pattern exceeds string length limits")
            throw e;
        return true;
    }
}

function testRegExpResult(regexpExpression, expectedResult)
{
    let result = eval(regexpExpression);
    if (JSON.stringify(result) !== JSON.stringify(expectedResult))
        throw "Expected \"" + regexpExpression + "\" to return " + JSON.stringify(expectedResult) + ", got: " + JSON.stringify(result);
}

// These patterns cause offset overflow and should throw
testRegExpThrows("/a{2147483649,2147483650}a{2147483649,2147483650}/.exec('aaaa')");
testRegExpThrows("/a{2147483649,2147483650}a{2147483649,2147483650}/.exec('aa')");

// These patterns are now supported with variable count parentheses and return valid results
testRegExpResult("/(?:\\1{2147483649,2147483650})+/.exec('123')", null);
testRegExpResult("/([^]{2147483648,2147483651}(?:.){2})+?/.exec('xxx')", null);
testRegExpResult("/(\\u0004\\W\\u0f0b+?$[\\xa7\\t\\t-\\ue118\\f]{2147483648,2147483648})+.+?/u.exec('testing')", null);
testRegExpResult("/(.{2147483649,2147483652})+?/g.exec('xxx')", null);
testRegExpResult("/(?:(?:[\\D]{2147483649})+?.)*?/igmy.exec('123\\n123')", [""]);
testRegExpResult("/(?:\\1{2147483648,})+?/m.exec('xxx')", null);

// Regression test for 159954.  This test should not crash or throw an exception.

function testRegExp(regexpExpression)
{
    try {
        let result = eval(regexpExpression);

        throw "Expected \"" + regexpExpression + "\" to throw and it didn't";
    } catch (e) {
        if (e != "SyntaxError: Invalid regular expression: pattern exceeds string length limits")
            throw e;
        return true;
    }
}

testRegExp("/a{2147483649,2147483650}a{2147483649,2147483650}/.exec('aaaa')");
testRegExp("/a{2147483649,2147483650}a{2147483649,2147483650}/.exec('aa')");
testRegExp("/(?:\1{2147483649,2147483650})+/.exec('123')");
testRegExp("/([^]{2147483648,2147483651}(?:.){2})+?/.exec('xxx')");
testRegExp("/(\u0004\W\u0f0b+?$[\xa7\t\t-\ue118\f]{2147483648,2147483648})+.+?/u.exec('testing')");
testRegExp("/(.{2147483649,2147483652})+?/g.exec('xxx')");
testRegExp("/(?:(?:[\D]{2147483649})+?.)*?/igmy.exec('123\\n123')");
testRegExp("/(?:\1{2147483648,})+?/m.exec('xxx')");

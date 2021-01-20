description("test that comparison operators work correctly.")

function makeTest(start, end, expression, relationship, override, invert) {
    var resultValue = eval(relationship + expression + 0) || !!override;
    if (invert)
        resultValue = !resultValue;
    var expr = start + expression + end;
    var result = [];
    function func(content) { var f = new Function(content); f.toString = function(){ return content}; return f; } 
    result.push([new func("return " + expr + ";"), resultValue]);
    result.push([new func("if (" + expr + ") return true; return false;"), resultValue]);
    result.push([new func("var k = 0; while (" + expr + ") if (k++) return true; return false;"), resultValue]);
    result.push([new func("var k = 0; for (; " + expr + "; ) if (k++) return true; return false;"), resultValue]);
    return result;
}
function doTest(lhs, rhs, relationship) {
    var expressionParts = [["(",")"],["(", ") || 1", true],["(", ") && 1"],["(", ") || 1", true],["1 || (",")", true],["1 && (",")"]];
    var expressions = [];
    var tests = [];
    for (var i = 0; i < expressionParts.length; i++) {
        var start = expressionParts[i][0] + lhs;
        var end = String(rhs) + expressionParts[i][1];
        tests.push.apply(tests, makeTest(start, end, "==", relationship, expressionParts[i][2]));
        tests.push.apply(tests, makeTest(start, end, "!=", relationship, expressionParts[i][2]));
        tests.push.apply(tests, makeTest(start, end, "===", relationship, expressionParts[i][2]));
        tests.push.apply(tests, makeTest(start, end, "!==", relationship, expressionParts[i][2]));
    }
    for (var i = 0; i < tests.length; i++) {
        if ((r=tests[i][0]()) == tests[i][1])
            testPassed(tests[i][0] + " is " + tests[i][1]);
        else
            testFailed(tests[i][0] + " is " + r + " and should be " + tests[i][1] + ".");
    }
}

var letterA = "a";
var letterB = "b";
var One = 1;
var Zero = 0;
doTest('"a"', '"b"', -1);
doTest('"a"', '"a"', 0);
doTest('"b"', '"a"', 1);
doTest('letterA', '"b"', -1);
doTest('letterA', '"a"', 0);
doTest('"b"', '"a"', 1);
doTest('letterA', '"b"', -1);
doTest('letterA', 'letterA', 0);
doTest('"b"', 'letterA', 1);
doTest('"a"', '"b"', -1);
doTest('"a"', 'letterA', 0);
doTest('"b"', 'letterA', 1);

doTest('"a"', '0', NaN);
doTest('0', '"a"', NaN);
doTest('letterA', '0', NaN);
doTest('letterA', '"a"', 0);
doTest('0', '"a"', NaN);
doTest('letterA', 'letterA', 0);
doTest('0', 'letterA', NaN);
doTest('"a"', 'letterA', 0);
doTest('0', 'letterA', NaN);


doTest('0', '1', -1);
doTest('0', '0', 0);
doTest('1', '0', 1);
doTest('Zero', '1', -1);
doTest('Zero', '0', 0);
doTest('1', 'Zero', 1);
doTest('0', 'One', -1);
doTest('One', '0', 1);

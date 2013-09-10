if (this.testRunner)
    testRunner.dumpAsText();

if (this.document) {
    log = function(msg) {document.getElementById("log").innerHTML += msg + "<br />";};
    fail = function(msg) {log("<span style='color: red'>FAIL: </span>" + msg) };
    pass = function(msg) { passCount++; log("<span style='color: green'>PASS: </span>" + msg) };
} else {
    log = print;
    fail = function(msg) {log("FAIL: " + msg) };
    pass = function(msg) { passCount++; log("PASS: " + msg); };
}

var unaryOps = ['~', '+', '-', '++', '--'];
var binaryOps;
if (!binaryOps) 
    binaryOps = ['-', '+', '*', '/', '%', '|', '&', '^', '<', '<=', '>=', '>', '==', '!=', '<<', '>>'];

var valueOfThrower = { valueOf: function() { throw "throw from valueOf"; } }
var toStringThrower = { toString: function() { throw "throw from toString"; } }
var testCount = 0;
var passCount = 0;
function id(){}
function createTest(expr) {
    // For reasons i don't quite understand these tests aren't expected to throw
    if (expr == "valueOfThrower == rhsNonZeroNum") return id;
    if (expr == "toStringThrower == rhsNonZeroNum") return id;
    if (expr == "valueOfThrower != rhsNonZeroNum") return id;
    if (expr == "toStringThrower != rhsNonZeroNum") return id;
    if (expr == "valueOfThrower == rhsToStringThrower") return id;
    if (expr == "toStringThrower == rhsToStringThrower") return id;
    if (expr == "valueOfThrower != rhsToStringThrower") return id;
    if (expr == "toStringThrower != rhsToStringThrower") return id;

    // This creates a test case that ensures that a binary operand will execute the left hand expression first,
    // and will not execute the right hand side if the left hand side throws.
    var functionPrefix = "(function(){ executedRHS = false; var result = 'PASS'; try { result = ";
    var functionPostfix = "; fail('Did not throw exception with \"' + expr + '\"') } catch (e) { " +
                          " if (result != 'PASS' && executedRHS) { fail('\"'+expr+'\" threw exception, but modified assignment target and executed RHS'); " +
                          " } else if (result != 'PASS') { fail('\"'+expr+'\" threw exception, but modified assignment target'); " +
                          " } else if (executedRHS) { fail('\"'+expr+'\" threw exception, but executed right hand half of expression')" +
                          " } else { pass('Handled \"'+ expr +'\" correctly.') } } })";
    testCount++;
    try {
        return eval(functionPrefix + expr + functionPostfix);
    } catch(e) {
        throw new String(expr);
    }
}

function createTestWithRHSExec(expr) {
    // This tests that we evaluate the right hand side of a binary expression before we
    // do any type conversion with toString and/or valueOf which may throw.
    var functionPrefix = "(function(){ executedRHS = false; var result = 'PASS'; try { result = ";
    var functionPostfix = "; fail('Did not throw exception with \"' + expr + '\"') } catch (e) { " +
                          " if (result != 'PASS') { fail('\"'+expr+'\" threw exception, but modified assignment target'); " +
                          " } else if (!executedRHS) { fail('\"'+expr+'\" threw exception, and failed to execute RHS when expected')" +
                          " } else { pass('Handled \"'+ expr +'\" correctly.') } } })";
    testCount++;
    try {
        return eval(functionPrefix + expr + functionPostfix);
    } catch(e) {
        throw new String(expr);
    }
}

window.__defineGetter__('throwingProperty', function(){ throw "throwing resolve"; });

var throwingPropStr = 'throwingProperty';
var valueOfThrowerStr = 'valueOfThrower';
var toStringThrowerStr = 'toStringThrower';
var throwingObjGetter = '({get throwingProperty(){ throw "throwing property" }}).throwingProperty';

rhsNonZeroNum = { valueOf: function(){ executedRHS = true; return 1; } };
rhsZeroNum = { valueOf: function(){ executedRHS = true; return 0; } };
rhsToStringThrower = { toString: function(){ executedRHS = true; return 'string'; }};
getterThrower = { get value() { throw "throwing in getter"; }};
var getterThrowerStr = "getterThrower.value";
rhsGetterTester = { get value() { executedRHS = true; return 'string'; }};
var rhsGetterTesterStr = "rhsGetterTester.value";

for (var i = 0; i < binaryOps.length; i++) {
    var currentOp = binaryOps[i];
    var numVal = currentOp == '||' ? '0' : '1';
    createTest(numVal + " " + currentOp + " " + valueOfThrowerStr )();
    createTest(numVal + " " + currentOp + " " + toStringThrowerStr)();
    createTest(numVal + " " + currentOp + " " + throwingPropStr   )();
    createTest(numVal + " " + currentOp + " " + throwingObjGetter )();
    createTest(numVal + " " + currentOp + " " + getterThrowerStr )();

    createTest("'string' " + currentOp + " " + valueOfThrowerStr )();
    createTest("'string' " + currentOp + " " + toStringThrowerStr)();
    createTest("'string' " + currentOp + " " + throwingPropStr   )();
    createTest("'string' " + currentOp + " " + throwingObjGetter )();
    createTest("'string' " + currentOp + " " + getterThrowerStr )();
    
    numVal = currentOp == '||' ? 'rhsZeroNum' : 'rhsNonZeroNum';
    createTest(valueOfThrowerStr  + " " + currentOp + " " + numVal)();
    createTest(toStringThrowerStr + " " + currentOp + " " + numVal)();
    createTest(throwingPropStr    + " " + currentOp + " " + numVal)();
    createTest(throwingObjGetter  + " " + currentOp + " " + numVal)();
    createTest(getterThrowerStr  + " " + currentOp + " " + numVal)();
    createTest(valueOfThrowerStr  + " " + currentOp + " rhsToStringThrower")();
    createTest(toStringThrowerStr + " " + currentOp + " rhsToStringThrower")();
    createTest(throwingPropStr    + " " + currentOp + " rhsToStringThrower")();
    createTest(throwingObjGetter  + " " + currentOp + " rhsToStringThrower")();
    createTest(getterThrowerStr  + " " + currentOp + " rhsToStringThrower")();
    createTestWithRHSExec(valueOfThrowerStr  + " " + currentOp + " " + rhsGetterTesterStr)();
    createTestWithRHSExec(toStringThrowerStr + " " + currentOp + " " + rhsGetterTesterStr)();

}

log("Passed " + passCount + " of " + testCount + " tests.");
if (window.testRunner)
	testRunner.dumpAsText();

description(
'This test checks that there is a stack property at creation of each error object.'
);

function checkStack(errorObject) {
    if(errorObject.stack == undefined)
        testFailed(errorObject + " did not have a .stack property at creation time.");
    else if(typeof (errorObject.stack) != "string")
        testFailed(errorObject + " has a stack but it is not a String.");
    else if(errorObject.stack.length == 0)
        testFailed(errorObject + " has a stack but it is empty.");
    else
        testPassed(errorObject + " has .stack property at creation time.");
}

checkStack(new Error());
checkStack(new EvalError());
checkStack(new RangeError());
checkStack(new ReferenceError());
checkStack(new SyntaxError());
checkStack(new TypeError());
checkStack(new URIError());

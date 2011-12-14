description(

"This test checks that JavaScriptCore does not crash when uwinding the stack that includes a host function."

);

function twoHostFunctions() {
    var stack = [];
    stack.push({ "args": twoHostFunctions.arguments });
    stack.push({ "args": twoHostFunctions.arguments });
    testPassed("Two host functions called in a row.");
}

function arrayOperatorFunction(element) {
    return element + 5;
}

var myArray = new Array (0, 1, 2);
function hostCallsUser(array) {
    return array.map(arrayOperatorFunction);
}

function throwException() {
    throw "Exception thrown";
}

function hostAndException() {
    var stack = [];
    stack.push({ "args": hostAndException.arguments });
    throwException();
}

twoHostFunctions();
myArray = hostCallsUser(myArray);
shouldBe(myArray, new Array( 5, 6, 7 ) );

try {
    hostAndException();
} catch (e) {
    testPassed("Exception thrown and caught");
}


successfullyParsed = true;


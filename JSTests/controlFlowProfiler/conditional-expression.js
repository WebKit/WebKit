load("./driver/driver.js");

function foo(){ }
function bar(){ }
function baz(){ }

function testConditionalBasic(x) {
    return x ? 10 : 20;
}


testConditionalBasic(false);
checkBasicBlock(testConditionalBasic, "x", ShouldHaveExecuted);
checkBasicBlock(testConditionalBasic, "20", ShouldHaveExecuted);
checkBasicBlock(testConditionalBasic, "10", ShouldNotHaveExecuted);

testConditionalBasic(true);
checkBasicBlock(testConditionalBasic, "10", ShouldHaveExecuted);


function testConditionalFunctionCall(x, y) {
    x ? y ? foo() 
        : baz() 
        : bar()
}
testConditionalFunctionCall(false, false);
checkBasicBlock(testConditionalFunctionCall, "x ?", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "? y", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "bar", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, ": bar", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "y ?", ShouldNotHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "? foo", ShouldNotHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "foo", ShouldNotHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "baz", ShouldNotHaveExecuted);

testConditionalFunctionCall(true, false);
checkBasicBlock(testConditionalFunctionCall, "y ?", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "? foo", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, ": baz", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "baz", ShouldHaveExecuted);
checkBasicBlock(testConditionalFunctionCall, "foo", ShouldNotHaveExecuted);

testConditionalFunctionCall(true, true);
checkBasicBlock(testConditionalFunctionCall, "foo", ShouldHaveExecuted);

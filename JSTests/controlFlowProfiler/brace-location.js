load("./driver/driver.js");

function foo() {}
function bar() {}
function baz() {}

function testIf(x) {
    if (x < 10) { foo(); } else if (x < 20) { bar(); } else { baz() }
}
testIf(9);
// Note, the check will be against the basic block that contains the first matched character. 
// So in this following case, the block that contains the '{'.
checkBasicBlock(testIf, "{ foo", ShouldHaveExecuted);
// In this case, it will test the basic block that contains the ' ' character.
checkBasicBlock(testIf, " foo", ShouldHaveExecuted);
checkBasicBlock(testIf, "} else if", ShouldHaveExecuted);
checkBasicBlock(testIf, "else if", ShouldNotHaveExecuted);
checkBasicBlock(testIf, "{ bar", ShouldNotHaveExecuted);
checkBasicBlock(testIf, " bar", ShouldNotHaveExecuted);
checkBasicBlock(testIf, "else {", ShouldNotHaveExecuted);
checkBasicBlock(testIf, "{ baz", ShouldNotHaveExecuted);
checkBasicBlock(testIf, " baz", ShouldNotHaveExecuted);
testIf(21);
checkBasicBlock(testIf, "else if (x < 20)", ShouldHaveExecuted); 
checkBasicBlock(testIf, "{ bar", ShouldNotHaveExecuted); 
checkBasicBlock(testIf, " bar", ShouldNotHaveExecuted); 
checkBasicBlock(testIf, "else {", ShouldHaveExecuted); 
checkBasicBlock(testIf, "{ baz", ShouldHaveExecuted); 
checkBasicBlock(testIf, " baz", ShouldHaveExecuted); 
testIf(11);
checkBasicBlock(testIf, "{ bar", ShouldHaveExecuted); 
checkBasicBlock(testIf, " bar", ShouldHaveExecuted); 

function testForRegular(x) {
    for (var i = 0; i < x; i++) { foo(); } bar();
}
testForRegular(0);
checkBasicBlock(testForRegular, "{ foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForRegular, "} bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForRegular, " bar", ShouldHaveExecuted); 
testForRegular(1);
checkBasicBlock(testForRegular, "{ foo", ShouldHaveExecuted); 
checkBasicBlock(testForRegular, "} bar", ShouldHaveExecuted); 

function testForIn(x) {
    for (var i in x) { foo(); } bar();
}
testForIn({});
checkBasicBlock(testForIn, "{ foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForIn, "} bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForIn, " bar", ShouldHaveExecuted); 
testForIn({foo: 20});
checkBasicBlock(testForIn, "{ foo", ShouldHaveExecuted); 
checkBasicBlock(testForIn, "} bar", ShouldHaveExecuted); 

function testForOf(x) {
    for (var i of x) { foo(); } bar();
}
testForOf([]);
checkBasicBlock(testForOf, "{ foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForOf, " foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForOf, "} bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForOf, " bar", ShouldHaveExecuted); 
testForOf([20]);
checkBasicBlock(testForOf, "{ foo", ShouldHaveExecuted); 
checkBasicBlock(testForOf, "} bar", ShouldHaveExecuted); 

function testWhile(x) {
    var i = 0; while (i++ < x) { foo(); } bar();
}
testWhile(0);
checkBasicBlock(testWhile, "{ foo", ShouldNotHaveExecuted); 
checkBasicBlock(testWhile, " foo", ShouldNotHaveExecuted); 
checkBasicBlock(testWhile, "} bar", ShouldNotHaveExecuted); 
checkBasicBlock(testWhile, " bar", ShouldHaveExecuted); 
testWhile(1);
checkBasicBlock(testWhile, "{ foo", ShouldHaveExecuted); 
checkBasicBlock(testWhile, "} bar", ShouldHaveExecuted); 


// No braces tests.

function testIfNoBraces(x) {
    if (x < 10) foo(); else if (x < 20) bar(); else baz();
}
testIfNoBraces(9);
checkBasicBlock(testIfNoBraces, "foo", ShouldHaveExecuted);
checkBasicBlock(testIfNoBraces, " foo", ShouldHaveExecuted);
checkBasicBlock(testIfNoBraces, "; else if", ShouldHaveExecuted);
checkBasicBlock(testIfNoBraces, " else if", ShouldNotHaveExecuted);
checkBasicBlock(testIfNoBraces, " bar", ShouldNotHaveExecuted); 
checkBasicBlock(testIfNoBraces, "bar", ShouldNotHaveExecuted); 
checkBasicBlock(testIfNoBraces, "else baz", ShouldNotHaveExecuted); 
checkBasicBlock(testIfNoBraces, "baz", ShouldNotHaveExecuted); 
testIfNoBraces(21);
checkBasicBlock(testIfNoBraces, "else baz", ShouldHaveExecuted); 
checkBasicBlock(testIfNoBraces, "baz", ShouldHaveExecuted); 
checkBasicBlock(testIfNoBraces, "; else baz", ShouldNotHaveExecuted); 
checkBasicBlock(testIfNoBraces, "else if (x < 20)", ShouldHaveExecuted);
// Note that the whitespace preceding bar is part of the previous basic block.
// An if statement's if-true basic block text offset begins at the start offset
// of the if-true block, in this case, just the expression "bar()".
checkBasicBlock(testIfNoBraces, " bar", ShouldHaveExecuted); 
checkBasicBlock(testIfNoBraces, "bar", ShouldNotHaveExecuted); 
testIfNoBraces(11);
checkBasicBlock(testIfNoBraces, " bar", ShouldHaveExecuted); 
checkBasicBlock(testIfNoBraces, "bar", ShouldHaveExecuted); 

function testForRegularNoBraces(x) {
    for (var i = 0; i < x; i++) foo(); bar();
}
testForRegularNoBraces(0);
checkBasicBlock(testForRegularNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForRegularNoBraces, "foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForRegularNoBraces, "; bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForRegularNoBraces, " bar", ShouldHaveExecuted); 
testForRegularNoBraces(1);
checkBasicBlock(testForRegularNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForRegularNoBraces, "foo", ShouldHaveExecuted); 
checkBasicBlock(testForRegularNoBraces, " bar", ShouldHaveExecuted); 

function testForInNoBraces(x) {
    for (var i in x) foo(); bar();
}
testForInNoBraces({});
checkBasicBlock(testForInNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForInNoBraces, "foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForInNoBraces, "; bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForInNoBraces, " bar", ShouldHaveExecuted); 
testForInNoBraces({foo: 20});
checkBasicBlock(testForInNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForInNoBraces, "foo", ShouldHaveExecuted); 
checkBasicBlock(testForInNoBraces, "; bar", ShouldHaveExecuted); 

function testForOfNoBraces(x) {
    for (var i of x) foo(); bar();
}
testForOfNoBraces([]);
checkBasicBlock(testForOfNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForOfNoBraces, "foo", ShouldNotHaveExecuted); 
checkBasicBlock(testForOfNoBraces, "; bar", ShouldNotHaveExecuted); 
checkBasicBlock(testForOfNoBraces, " bar", ShouldHaveExecuted); 
testForOfNoBraces([20]);
checkBasicBlock(testForOfNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testForOfNoBraces, "foo", ShouldHaveExecuted); 
checkBasicBlock(testForOfNoBraces, "; bar", ShouldHaveExecuted); 

function testWhileNoBraces(x) {
    var i = 0; while (i++ < x) foo(); bar();
}
testWhileNoBraces(0);
checkBasicBlock(testWhileNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testWhileNoBraces, "foo", ShouldNotHaveExecuted); 
checkBasicBlock(testWhileNoBraces, "; bar", ShouldNotHaveExecuted); 
checkBasicBlock(testWhileNoBraces, " bar", ShouldHaveExecuted); 
testWhileNoBraces(1);
checkBasicBlock(testWhileNoBraces, " foo", ShouldHaveExecuted); 
checkBasicBlock(testWhileNoBraces, "foo", ShouldHaveExecuted); 
checkBasicBlock(testWhileNoBraces, "; bar", ShouldHaveExecuted); 

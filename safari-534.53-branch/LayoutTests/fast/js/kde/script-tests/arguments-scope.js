// We can't use normal shouldBe here, since they'd eval in the wrong context...

function shouldBeOfType(msg, val, type) {
  if (typeof(val) != type)
    testFailed(msg + ": value has type " + typeof(val) + " , not:" + type);
  else
    testPassed(msg);
}

function test0() {
    var arguments;
    // var execution should not overwrite something that was 
    // in scope beforehand -- e.g. the arguments thing
    shouldBeOfType("test0", arguments, 'object');
 }

function test1() {
    // No need to undef-initialize something in scope already!
    shouldBeOfType("test1", arguments, 'object');
    var arguments;
}

function test2(arguments) {
    // Formals OTOH can overwrite the args object
    shouldBeOfType("test2", arguments, 'number');
}


function test3() {
    // Ditto for functions..
    shouldBeOfType("test3", arguments, 'function');
    function arguments() {}
}

function test4() {
    // Here, the -declaration- part of the var below should have no 
    // effect..
    shouldBeOfType('test4.(1)', arguments, 'object');
    var arguments = 4;
    // .. but the assignment shoud just happen
    shouldBeOfType('test4.(2)', arguments, 'number');
}


test0();
test1();
test2(42);
test3();
test4();

var successfullyParsed = true;

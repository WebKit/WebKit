description(
"This test checks the behavior of Arguments object iteration."
);


var arguments = null;
shouldThrow("(function (arguments) { for (var argument of arguments) {}})()")

function test() {
    var i = 0;
    for (arg of arguments) {
        realArg = arguments[i++];
        shouldBeTrue("arg === realArg");
    }
    iteratedArgumentsLength = i;
    actualArgumentsLength = arguments.length;
    shouldBe("actualArgumentsLength", "iteratedArgumentsLength");
}

test();
test("a");
test("a", "b");
test({})

function testAlias() {
    var i = 0;
    var a = arguments;
    for (arg of a) {
        realArg = arguments[i++];
        shouldBeTrue("arg === realArg");
    }
    iteratedArgumentsLength = i;
    actualArgumentsLength = arguments.length;
    shouldBe("actualArgumentsLength", "iteratedArgumentsLength");
}

testAlias();
testAlias("a");
testAlias("a", "b");
testAlias({})


function testStrict() {
    "use strict";
    var i = 0;
    for (arg of arguments) {
        realArg = arguments[i++];
        shouldBeTrue("arg === realArg");
    }
    iteratedArgumentsLength = i;
    actualArgumentsLength = arguments.length;
    shouldBe("actualArgumentsLength", "iteratedArgumentsLength");
}

testStrict();
testStrict("a");
testStrict("a", "b");
testStrict({})


function testReifiedArguments() {
    var i = 0;
    arguments.expando = 1;
    for (arg of arguments) {
        realArg = arguments[i++];
        shouldBeTrue("arg === realArg");
    }
    iteratedArgumentsLength = i;
    actualArgumentsLength = arguments.length;
    shouldBe("actualArgumentsLength", "iteratedArgumentsLength");
}

testReifiedArguments();
testReifiedArguments("a");
testReifiedArguments("a", "b");
testReifiedArguments({})


function testOverwrittenArguments() {
    var i = 0;
    arguments = "foobar";
    for (arg of arguments) {
        realArg = arguments[i++];
        shouldBeTrue("arg === realArg");
    }
    iteratedArgumentsLength = i;
    actualArgumentsLength = arguments.length;
    shouldBe("actualArgumentsLength", "iteratedArgumentsLength");
}


testOverwrittenArguments();
testOverwrittenArguments("a");
testOverwrittenArguments("a", "b");
testOverwrittenArguments({})



function testNullArguments() {
    var i = 0;
    arguments = null;
    for (arg of arguments) {
        fail("nothing to iterate");
    }
}

shouldThrow("testNullArguments()");
function testNonArrayLikeArguments() {
    var i = 0;
    arguments = {};
    for (arg of arguments) {
        fail("nothing to iterate");
        return false;
    }
    return true;
}
shouldBeTrue("testNonArrayLikeArguments()");


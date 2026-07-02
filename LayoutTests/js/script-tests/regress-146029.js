description("Verify that we don't trash m_currentInstruction with an inlined function.");

function myPush(a, o) {
    a.push(o);
}

function myPop(a) {
    a.pop();
}

function foo(a) {
    myPush(a, 42);
    myPop(a);
    return a.length;
}

noInline(foo);

function test() {
    var myArray = ["one", "two", "three"];

    for (var i = 0; i < 10000; ++i) {
        if (foo(myArray) != 3) {
            testFailed("Array changed unexpectedly");
            return false;
        }
    }
    return true;
}

if (test())
   testPassed("Correctly inlined callee and used m_currentInstruction in caller");

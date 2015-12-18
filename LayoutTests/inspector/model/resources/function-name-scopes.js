function testFunctionNameScope1() {
    (function functionName() {
        debugger;
    })();
}

function testFunctionNameScope2() {
    (class MyClass {
        static staticMethod() {
            debugger;
        }
    }).staticMethod();
}

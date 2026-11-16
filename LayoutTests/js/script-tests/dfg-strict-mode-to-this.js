description("Tests that doing to-this conversion in strict mode doesn't cause us to believe that if the input is an object then the output is also an object.");

function thingy() {
    "use strict";
    function bar() {
        return this instanceof Object;
    }
    function foo() {
        return bar();
    }
    return foo();
}

dfgShouldBe(thingy, "thingy()", "false");

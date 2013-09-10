description(
"This test ensures that properties of nested function code are not lost (due to cached function info)."
);

var passed1 = false;
var passed2 = false;
var passed3 = false;
var passed4 = false;

// Test cases deliberately nested!
function runTests() {
    // Formating of these functions is significant for regression
    // testing; functions with small bodies are not cached!
    function test1() { return this; }
    function test2() { "use strict"; return this; }
    function test3() {
        return this ? "OKAY" : "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
    }
    function test4() {
        "use strict";
        return !this ? "OKAY" : "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
    }

    passed1 = test1() === this;
    passed2 = test2() === undefined;
    passed3 = test3() === "OKAY";
    passed4 = test4() === "OKAY";
};

runTests();
shouldBeTrue("passed1");
shouldBeTrue("passed2");
shouldBeTrue("passed3");
shouldBeTrue("passed4");

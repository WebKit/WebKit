description("Test to ensure that the registerfile is grown correctly when calling apply");

function testLog() { testPassed(this); }
(function () {
    Function.prototype.call.apply(testLog, arguments);
})('Did not crash using apply', 0, 0); // needs 3+ arguments
(function () {
    arguments; // reify the arguments object.
    Function.prototype.call.apply(testLog, arguments);
})('Did not crash using apply', 0, 0); // needs 3+ arguments

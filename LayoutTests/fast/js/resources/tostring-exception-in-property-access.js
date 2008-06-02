description("This tests that the various subscript operators handle subscript string conversion exceptions correctly.");

var toStringThrower = { toString: function() { throw "Exception thrown by toString"; }};
var target = {"" : "Did not assign to property when setter subscript threw"};

try {
    target[toStringThrower] = "Assigned to property on object when subscript threw";
} catch(e) {
    testPassed("PASS: Exception caught -- " + e);
}
shouldBe('target[""]', "'Did not assign to property when setter subscript threw'");

target[""] = "Did not delete property when subscript threw";
try {
    delete target[toStringThrower];
} catch(e) {
    testPassed("PASS: Exception caught -- " + e);
}
shouldBe('target[""]', "'Did not delete property when subscript threw'");

delete target[""];

target.__defineGetter__("", function(){
                                testFailed('FAIL: Loaded property from object when subscript threw.');
                                return "FAIL: Assigned to result when subscript threw.";
                            });
var localTest = "Did not assign to result when subscript threw.";
try {
    localTest = target[toStringThrower];
} catch(e) {
    testPassed("PASS: Exception caught -- " + e);
}
shouldBe('localTest', "'Did not assign to result when subscript threw.'");
var successfullyParsed = true;

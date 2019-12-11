description(
"This tests times that shouldn't happen because of DST, or times that happen twice"
);

description(
"For times that happen twice the behavior of all major browsers seems to be to pick the second occurrance, i.e. Standard Time not Daylight Time"
);

var testCases = [];
if ((new Date(2014, 8, 1)).toString().match("PDT")) {
    testCases.push(["(new Date('Mar 09 2014 03:00:00')).getHours()", "3"]);
    testCases.push(["(new Date('Mar 09 2014 03:00:00')).getTimezoneOffset()", "420"]);
    testCases.push(["(new Date('Nov 02 2014 01:00:00')).getHours()", "1"]);
    testCases.push(["(new Date('Nov 02 2014 01:00:00')).getTimezoneOffset()", "480"]);
}

var errors = [];
for (var i = 0; i < testCases.length; i++) {
    var actual = eval(testCases[i][0]);
    var expected = eval(testCases[i][1]);
    if (actual != expected) {
        errors.push(testCases[i][0] + " should be " + testCases[i][1] + ". Was " + actual + ".");
    }
}

if (errors.length) {
    testFailed(errors.length + "/" + testCases.length + " tests were failed: " + errors.join(", "));
} else {
    testPassed("Passed all tests (or skipped all tests if your timezone isn't PST/PDT)");
}

description(
"This tests times that shouldn't happen because of DST, or times that happen twice"
);

description(
"For times that shouldn't happen we currently go back an hour, but in reality we would like to go forward an hour.  This has been filed as a radar: 4777813"
);

description(
"For times that happen twice the behavior of all major browsers seems to be to pick the second occurance, i.e. Standard Time not Daylight Time"
);

var testCases = [];
if ((new Date(2009, 9, 1)).toString().match("PDT")) {
    testCases.push(["(new Date(1982, 2, 14, 2, 10)).getHours()", "1"]);
    testCases.push(["(new Date(1982, 2, 14, 2)).getHours()", "1"]);
    testCases.push(["(new Date(1982, 11, 7, 1, 10)).getTimezoneOffset()", "480"]);
    testCases.push(["(new Date(1982, 11, 7, 1)).getTimezoneOffset()", "480"]);
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

var successfullyParsed = true;

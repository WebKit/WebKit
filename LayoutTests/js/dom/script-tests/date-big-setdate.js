description(
'This test checks for regression against: <a href="http://bugs.webkit.org/show_bug.cgi?id=3381"> 3381 Date.prototype.setDate() incorrect for values >=128.</a> <br /> <a href="http://bugs.webkit.org/show_bug.cgi?id=12975">12975: DST changes in US affect JavaScript date calculations</a>'
);


var validVars = false;
var curValue;
var success = true;
var millisecondsPerDay = 1000 * 60 * 60 * 24;
var millisecondsPerHour = 1000 * 60 * 60;

for (var i = 116; i < 126; i++) {
    var d = new Date(0);
    d.setDate(i);
    if (validVars)
        shouldBe("d.valueOf() - curValue", "millisecondsPerDay");

    curValue = d.valueOf();        
    validVars = true;
}

var testCases = [];
if ((new Date(2009, 9, 1)).toString().match("PDT")) {
    // Added a special case that should represent a change in DST.  DST did not actually
    // change on this date but because of the wierdness of how JavaScriptDates are
    // expected to interpolate DST as opposed to reflect acurate history, this day
    // (April 5th 1970) should show a DST change.
    testCases.push([new Date(0), 146, 147]); // DST in 1970 is 4/26 14:00 - 10/25 14:00. And since epoch 0 is 16:00, 147 dates (4/26) is already in DST.

    // Added more special cases. These dates match the recent DST changes in the US.
    // These tests check that the new changes are correctly propogated to the past and
    // all of the tests should show DST occurring on the same date.
    testCases.push([new Date(1970, 0,0,0,0,0,0), 147, 148]); // DST in 1970 is 4/26 14:00 - 10/25 14:00
    testCases.push([new Date(1998, 0,0,0,0,0,0), 126, 127]); // DST in 1998 is 4/5 14:00 - 10/25 14:00
    testCases.push([new Date(2026, 0,0,0,0,0,0), 98, 99]); // DST in 2026 is 3/8 14:00 - 11/1 14:00
    testCases.push([new Date(2054, 0,0,0,0,0,0), 98, 99]); // DST in 2054 is 3/8 14:00 - 11/1 14:00
}

var errors = [];
for (var i = 0; i < testCases.length; i++) {
    var c = testCases[i][0];
    var d = new Date(c);
    c.setDate(testCases[i][1]);
    d.setDate(testCases[i][2]);
    var actual = d.valueOf() - c.valueOf();
    var expected = millisecondsPerDay - millisecondsPerHour;
    if (actual != expected) {
        errors.push("Unexpected difference between two days (expected: " + expected + ", actual: " +  actual + ") for " + testCases[i][0]);
    }
}

if (errors.length) {
    testFailed(errors.length + "/" + testCases.length + " tests were failed: " + errors.join(", "));
} else {
    testPassed("Passed all tests for DST (or skipped the tests if your timezone isn't PST/PDT)");
}

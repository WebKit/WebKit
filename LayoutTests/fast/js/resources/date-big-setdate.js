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

// Added a special case that should represent a change in DST.  DST did not actually
// change on this date but because of the wierdness of how JavaScriptDates are
// expected to interpolate DST as opposed to reflect acurate history, this day
// (April 5th 1970) should show a DST change.

var c = new Date(0);
var d = new Date(0);
c.setDate(97);
d.setDate(98);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

// Added more special cases. These dates match the recent DST changes in the US.
// These tests check that the new changes are correctly propogated to the past and
// all of the tests should show DST occurring on the same date.  

var c = new Date(1970, 0,0,0,0,0,0);
var d = new Date(1970, 0,0,0,0,0,0);
c.setDate(98);
d.setDate(99);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

var c = new Date(1998, 0,0,0,0,0,0);
var d = new Date(1998, 0,0,0,0,0,0);
c.setDate(98);
d.setDate(99);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

var c = new Date(2026, 0,0,0,0,0,0);
var d = new Date(2026, 0,0,0,0,0,0);
c.setDate(98);
d.setDate(99);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

var c = new Date(2054, 0,0,0,0,0,0);
var d = new Date(2054, 0,0,0,0,0,0);
c.setDate(98);
d.setDate(99);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

var successfullyParsed = true;

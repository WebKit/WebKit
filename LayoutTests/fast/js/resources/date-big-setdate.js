description(
'This test checks for regression against: <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=3381"> 3381 Date.prototype.setDate() incorrect for values >=128.</a>'
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
c.setDate(125);
d.setDate(126);
shouldBe("d.valueOf() - c.valueOf()", "millisecondsPerDay - millisecondsPerHour");

var successfullyParsed = true;

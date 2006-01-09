description(
'This test checks for regression against: <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=3381"> 3381 Date.prototype.setDate() incorrect for values >=128.</a>'
);


var validVars = false;
var curValue;
var success = true;
var millisecondsPerDay = 1000 * 60 * 60 * 24;
	
for (var i = 120; i < 130; i++) {
    var d = new Date(0);
    d.setDate(i);
    if (validVars)
        shouldBe("d.valueOf() - curValue", "millisecondsPerDay");

    curValue = d.valueOf();        
    validVars = true;
}

var successfullyParsed = true;

description(
'This test checks for a regression against <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5280">Date.setMonth fails with negative values</a>.'
);

var d = new Date(2005, 6, 15);
d.setMonth(-3);
shouldBe("d.valueOf()", "new Date(2004, 9, 15).valueOf()");

var successfullyParsed = true;

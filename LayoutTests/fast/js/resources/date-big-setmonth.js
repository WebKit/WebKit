description(
'This test checks for a regression against <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=4781">Date.setMonth fails with big values due to overflow</a>.'
);

var d = new Date(1970, 0, 1);
d.setMonth(128);
shouldBe("d.valueOf()", "new Date(1980, 8, 1).valueOf()");

var successfullyParsed = true;

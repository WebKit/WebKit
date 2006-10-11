description(
"This tests times that shouldn't happen because of DST, or times that happen twice"
);

description(
"For times that shouldn't happen we currently go back an hour, but in reality we would like to go forward an hour.  This has been filed as a radar: 4777813"
);

description(
"For times that happen twice the behavior of all major browsers seems to be to pick the second occurance, i.e. Standard Time not Daylight Time"
);

shouldBe("(new Date(1982, 3, 25, 2, 10)).getHours()", "1");
shouldBe("(new Date(1982,3,25,2)).getHours()", "1");
shouldBe("(new Date(1982, 9, 31, 1, 10)).getTimezoneOffset()", "480");
shouldBe("(new Date(1982, 9, 31, 1)).getTimezoneOffset()", "480");

var successfullyParsed = true;
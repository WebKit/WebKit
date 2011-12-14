description(
"This tests the DST status in dates earlier than 1970-01-01 00:00 UTC. Regardless of your time zone, it should not fail. This tests time zones: US/Pacific, US/Central, US/Mountain, US/Eastern, CET, Asia/Jerusalem and NZ."
);

shouldBe("(new Date(1970, 0, 1)).getHours()", "0");
shouldBe("(new Date(1969, 8, 1)).getHours()", "0");
shouldBe("(new Date(1969, 9, 28)).getHours()", "0");

var successfullyParsed = true;

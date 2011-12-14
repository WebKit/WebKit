description(
'Test for beginning of line (BOL or ^) matching in a multiline string</a>'
);

var s = "aced\nabc";
shouldBeNull('s.match(/^abc/)');
shouldBe('s.match(/^abc/m)', '["abc"]');
shouldBeNull('s.match(/(^|X)abc/)');
shouldBe('s.match(/(^|X)abc/m)', '["abc",""]');
shouldBe('s.match(/(^a|Xa)bc/m)', '["abc","a"]');

var successfullyParsed = true;

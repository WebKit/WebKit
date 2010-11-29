description(
"This page tests invalid character ranges in character classes."
);

// These test a basic range / non range.
shouldBe('/[a-c]+/.exec("-acbd");', '["acb"]');
shouldBe('/[a\\-c]+/.exec("-acbd")', '["-ac"]');

// A reverse-range is invalid.
shouldThrow('/[c-a]+/.exec("-acbd");');

// A character-class in a range is invalid.
shouldThrow('/[\\d-x]+/.exec("1-3xy");');
shouldThrow('/[x-\\d]+/.exec("1-3xy");');
shouldThrow('/[\\d-\\d]+/.exec("1-3xy");');

// An escaped hypen should not be confused for an invalid range.
shouldBe('/[\\d\\-x]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[x\\-\\d]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[\\d\\-\\d]+/.exec("1-3xy");', '["1-3"]');

// A hyphen after a character-class is not invalid.
shouldBe('/[\\d-]+/.exec("1-3xy")', '["1-3"]');

var successfullyParsed = true;

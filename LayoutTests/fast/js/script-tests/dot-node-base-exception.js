description(
"This test checks that a correct exception is raised when calculating the base value of a dot expression fails."
);

// Should be a DOM exception, not just some "TypeError: Null value".
shouldThrow('(document.appendChild()).foobar()', '"Error: NOT_FOUND_ERR: DOM Exception 8"');

var successfullyParsed = true;

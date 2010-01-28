description('Tests for .valueAsNumber with &lt;input type=date>.');

var input = document.createElement('input');
input.type = 'date';

function valueAsNumberFor(stringValue) {
    input.value = stringValue;
    return input.valueAsNumber;
}

function setValueAsNumberAndGetValue(year, month, day) {
    input.valueAsNumber = Date.UTC(year, month, day);
    return input.value;
}

shouldBe('valueAsNumberFor("")', 'Number.NaN');
shouldBe('valueAsNumberFor("1969-12-31")', 'Date.UTC(1969, 11, 31)');
shouldBe('valueAsNumberFor("1970-01-01")', 'Date.UTC(1970, 0, 1)');
shouldBe('valueAsNumberFor("2009-12-22")', 'Date.UTC(2009, 11, 22)');

shouldBe('setValueAsNumberAndGetValue(1969, 11, 1)', '"1969-12-01"');
shouldBe('setValueAsNumberAndGetValue(1970, 0, 1)', '"1970-01-01"');
shouldBe('setValueAsNumberAndGetValue(2009, 11, 31)', '"2009-12-31"');
shouldBe('setValueAsNumberAndGetValue(10000, 0, 1)', '"10000-01-01"');

shouldBe('setValueAsNumberAndGetValue(794, 9, 22)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 8, 30)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 9, 15)', '"1582-10-15"');
shouldBe('setValueAsNumberAndGetValue(1582, 9, 31)', '"1582-10-31"');
shouldBe('setValueAsNumberAndGetValue(275760, 8, 13)', '"275760-09-13"');
// Date.UTC() of V8 throws an exception for the following value though JavaScriptCore doesn't.
// shouldBe('setValueAsNumberAndGetValue(275760, 8, 14)', '"275760-09-14"');

debug('Tests to set invalid values to valueAsNumber:');
shouldBe('input.value = ""; input.valueAsNumber = null; input.value', '"1970-01-01"');
shouldThrow('input.valueAsNumber = "foo"', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Infinity', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.POSITIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NEGATIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');

var successfullyParsed = true;

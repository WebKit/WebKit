description('Tests for .valueAsNumber with &lt;input type=week>.');

var input = document.createElement('input');
input.type = 'week';

function valueAsNumberFor(stringValue) {
    input.value = stringValue;
    return input.valueAsNumber;
}

function setValueAsNumberAndGetValue(year, month, day) {
    input.valueAsNumber = Date.UTC(year, month, day);
    return input.value;
}

shouldBe('valueAsNumberFor("")', 'Number.NaN');
// January 1st is Monday. W01 starts on January 1nd.
shouldBe('valueAsNumberFor("2007-W01")', 'Date.UTC(2007, 0, 1)');
// January 1st is Tuesday. W01 starts on December 31th.
shouldBe('valueAsNumberFor("2008-W01")', 'Date.UTC(2007, 11, 31)');
// January 1st is Wednesday. W01 starts on December 29th.
shouldBe('valueAsNumberFor("2003-W01")', 'Date.UTC(2002, 11, 30)');
// January 1st is Thursday. W01 starts on December 29th.
shouldBe('valueAsNumberFor("2004-W01")', 'Date.UTC(2003, 11, 29, 0, 0, 0, 0)');
// January 1st is Friday. W01 starts on January 4th.
shouldBe('valueAsNumberFor("2010-W01")', 'Date.UTC(2010, 0, 4)');
// January 1st is Saturday. W01 starts on January 3rd.
shouldBe('valueAsNumberFor("2005-W01")', 'Date.UTC(2005, 0, 3)');
// January 1st is Sunday. W01 starts on January 2nd.
shouldBe('valueAsNumberFor("2006-W01")', 'Date.UTC(2006, 0, 2)');

debug('Various January 1st:');
shouldBe('setValueAsNumberAndGetValue(2007, 0, 1)', '"2007-W01"');
shouldBe('setValueAsNumberAndGetValue(2008, 0, 1)', '"2008-W01"');
shouldBe('setValueAsNumberAndGetValue(2003, 0, 1)', '"2003-W01"');
shouldBe('setValueAsNumberAndGetValue(2004, 0, 1)', '"2004-W01"');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 1)', '"2009-W53"');
shouldBe('setValueAsNumberAndGetValue(2005, 0, 1)', '"2004-W53"');
shouldBe('setValueAsNumberAndGetValue(2006, 0, 1)', '"2005-W52"');

debug('Normal cases:');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 3)', '"2009-W53"');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 4)', '"2010-W01"');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 10)', '"2010-W01"');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 11)', '"2010-W02"');
shouldBe('setValueAsNumberAndGetValue(2010, 0, 17)', '"2010-W02"');
shouldBe('setValueAsNumberAndGetValue(2010, 11, 31)', '"2010-W52"');

debug('Around Gregorian calendar starting year:');
// Gregorian calendar started in 1582. We don't support that year.
shouldBe('setValueAsNumberAndGetValue(1582, 9, 14)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 9, 15)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 11, 31)', '""');
// January 1st is Saturday. W01 starts on January 3rd.
shouldBe('setValueAsNumberAndGetValue(1583, 0, 1)', '""');
shouldBe('setValueAsNumberAndGetValue(1583, 0, 2)', '""');
shouldBe('setValueAsNumberAndGetValue(1583, 0, 3)', '"1583-W01"');

debug('Tests to set invalid values to valueAsNumber:');
shouldBe('input.value = ""; input.valueAsNumber = null; input.value', '"1970-W01"');
shouldThrow('input.valueAsNumber = "foo"', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Infinity', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.POSITIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NEGATIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');

var successfullyParsed = true;

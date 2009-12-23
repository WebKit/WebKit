description('Tests for .valueAsDate with &lt;input type=week>.');

var input = document.createElement('input');
input.type = 'week';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

shouldBe('valueAsDateFor("")', 'null');
// January 1st is Monday. W01 starts on January 1nd.
shouldBe('valueAsDateFor("2007-W01").getTime()', 'Date.UTC(2007, 0, 1)');
// January 1st is Tuesday. W01 starts on December 31th.
shouldBe('valueAsDateFor("2008-W01").getTime()', 'Date.UTC(2007, 11, 31)');
// January 1st is Wednesday. W01 starts on December 29th.
shouldBe('valueAsDateFor("2003-W01").getTime()', 'Date.UTC(2002, 11, 30)');
// January 1st is Thursday. W01 starts on December 29th.
shouldBe('valueAsDateFor("2004-W01").getTime()', 'Date.UTC(2003, 11, 29, 0, 0, 0, 0)');
// January 1st is Friday. W01 starts on January 4th.
shouldBe('valueAsDateFor("2010-W01").getTime()', 'Date.UTC(2010, 0, 4)');
// January 1st is Saturday. W01 starts on January 3rd.
shouldBe('valueAsDateFor("2005-W01").getTime()', 'Date.UTC(2005, 0, 3)');
// January 1st is Sunday. W01 starts on January 2nd.
shouldBe('valueAsDateFor("2006-W01").getTime()', 'Date.UTC(2006, 0, 2)');

var successfullyParsed = true;

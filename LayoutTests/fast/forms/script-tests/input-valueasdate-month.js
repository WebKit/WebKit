description('Tests for .valueAsDate with &lt;input type=month>.');

var input = document.createElement('input');
input.type = 'month';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

shouldBe('valueAsDateFor("")', 'null');
shouldBe('valueAsDateFor("1969-12").getTime()', 'Date.UTC(1969, 11, 1, 0, 0, 0, 0)');
shouldBe('valueAsDateFor("1970-01").getTime()', 'Date.UTC(1970, 0, 1)');
shouldBe('valueAsDateFor("2009-12").getTime()', 'Date.UTC(2009, 11, 1)');

var successfullyParsed = true;

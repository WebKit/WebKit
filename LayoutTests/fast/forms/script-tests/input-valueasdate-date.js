description('Tests for .valueAsDate with &lt;input type=date>.');

var input = document.createElement('input');
input.type = 'date';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

shouldBe('valueAsDateFor("")', 'null');
shouldBe('valueAsDateFor("1969-12-31").getTime()', 'Date.UTC(1969, 11, 31)');
shouldBe('valueAsDateFor("1970-01-01").getTime()', 'Date.UTC(1970, 0, 1)');
shouldBe('valueAsDateFor("2009-12-22").getTime()', 'Date.UTC(2009, 11, 22)');

var successfullyParsed = true;

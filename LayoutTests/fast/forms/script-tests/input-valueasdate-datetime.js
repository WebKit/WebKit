description('Tests for .valueAsDate with &lt;input type=datetime>.');

var input = document.createElement('input');
input.type = 'datetime';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

shouldBe('valueAsDateFor("")', 'null');
shouldBe('valueAsDateFor("1969-12-31T12:34:56.789Z").getTime()', 'Date.UTC(1969, 11, 31, 12, 34, 56, 789)');
shouldBe('valueAsDateFor("1970-01-01T00:00:00.000Z").getTime()', 'Date.UTC(1970, 0, 1, 0, 0, 0)');
shouldBe('valueAsDateFor("2009-12-22T11:32:11Z").getTime()', 'Date.UTC(2009, 11, 22, 11, 32, 11)');

var successfullyParsed = true;

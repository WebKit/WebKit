description('Tests for .valueAsDate with &lt;input type=time>.');

var input = document.createElement('input');
input.type = 'time';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

shouldBe('valueAsDateFor("")', 'null');
shouldBe('valueAsDateFor("00:00:00.000").getTime()', 'Date.UTC(1970, 0, 1, 0, 0, 0, 0)');
shouldBe('valueAsDateFor("04:35").getTime()', 'Date.UTC(1970, 0, 1, 4, 35, 0, 0)');
shouldBe('valueAsDateFor("23:59:59.999").getTime()', 'Date.UTC(1970, 0, 1, 23, 59, 59, 999)');

var successfullyParsed = true;

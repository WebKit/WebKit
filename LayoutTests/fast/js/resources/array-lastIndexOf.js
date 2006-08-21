description(
'This test checks lastIndexOf for various values in an array'
);


var testArray = [2, 5, 9, 2];
var lastIndex = 0;

lastIndex = testArray.lastIndexOf(2,-500);
shouldBe('lastIndex', '-1');
lastIndex = testArray.lastIndexOf(9,500);
shouldBe('lastIndex', '2');
lastIndex = testArray.lastIndexOf(2);
shouldBe('lastIndex', '3');
lastIndex = testArray.lastIndexOf(7);
shouldBe('lastIndex', '-1');
lastIndex = testArray.lastIndexOf(2, 3);
shouldBe('lastIndex', '3');
lastIndex = testArray.lastIndexOf(2, 2);
shouldBe('lastIndex', '0');
lastIndex = testArray.lastIndexOf(2, -2);
shouldBe('lastIndex', '0');
lastIndex = testArray.lastIndexOf(2, -1);
shouldBe('lastIndex', '3');

var successfullyParsed = true;

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

delete testArray[1];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

delete testArray[3];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

testArray = new Array(20);

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

testArray[19] = undefined;

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '19');

lastIndex = testArray.lastIndexOf(undefined, 18);
shouldBe('lastIndex', '-1');

delete testArray[19];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

var successfullyParsed = true;

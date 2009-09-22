description(
"This test puts an item in a big index and then tries to change it. It shoudl change successfully."
);

var array = [];
array[10001] = 0;
array[10001] = 5;
array[10002] = "a";
array[10002] = "b";

shouldBe('array[10001]', '5');
shouldBe('array[10002]', '"b"');

var successfullyParsed = true;

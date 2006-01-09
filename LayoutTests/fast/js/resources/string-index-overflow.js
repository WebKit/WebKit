description(
"This test checks that accessing a string by an out of bounds index doesn't crash, furthermore the string should not appear to have out-of-bounds numeric properties."
);

shouldBeUndefined('"x"[10]');
shouldBeFalse('"x".hasOwnProperty(10)');

var successfullyParsed = true;

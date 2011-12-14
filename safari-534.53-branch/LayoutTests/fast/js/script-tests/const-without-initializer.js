description(
'Tests that declaring a const variable without initializing has the correct behavior and does not crash'
);

const f;

shouldBe('f', 'undefined');

f = 10;

shouldBe('f', 'undefined');

var successfullyParsed = true;

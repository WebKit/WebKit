description("This page tests iteration of properties on a string object.");

var stringProperties = new Array();
var i = 0;
for (var property in "abcde") {
    stringProperties[i++] = property;
}

shouldBe('stringProperties.length', '5');
shouldBe('stringProperties[0]', '"0"');
shouldBe('stringProperties[1]', '"1"');
shouldBe('stringProperties[2]', '"2"');
shouldBe('stringProperties[3]', '"3"');
shouldBe('stringProperties[4]', '"4"');

var successfullyParsed = true;

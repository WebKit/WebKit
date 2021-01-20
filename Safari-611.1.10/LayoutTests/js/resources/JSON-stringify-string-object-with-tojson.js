var stringObject = new String("Foo");
shouldBeEqualToString('nativeJSON.stringify(stringObject)', '"Foo"');
shouldBe('nativeJSON.stringify(stringObject)', 'JSON.stringify(stringObject)');

stringObject.toJSON = function() { return "Weird Case 1"; }
shouldBeEqualToString('nativeJSON.stringify(stringObject)', '"Weird Case 1"');
shouldBe('nativeJSON.stringify(stringObject)', 'JSON.stringify(stringObject)');

var stringObject = new String("Bar");
shouldBeEqualToString('nativeJSON.stringify(stringObject)', '"Bar"');
shouldBe('nativeJSON.stringify(stringObject)', 'JSON.stringify(stringObject)');

String.prototype.toJSON = function() { return "Weird Case 2"; }
shouldBeEqualToString('nativeJSON.stringify(stringObject)', '"Weird Case 2"');
shouldBe('nativeJSON.stringify(stringObject)', 'JSON.stringify(stringObject)');
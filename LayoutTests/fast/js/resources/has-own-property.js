description(
"This test verifies the behaviour of Object.prototype.hasOwnProperty, as documented in ECMA-262 rev3 section 15.2.4.5."
);

shouldBe("typeof {foo : 'yum'}.hasOwnProperty", '"function"');

shouldBeTrue("({foo : 'yum'}).hasOwnProperty('foo')");
shouldBeTrue("''.hasOwnProperty('length')");
shouldBeFalse("({foo : 'yum'}).hasOwnProperty('bar')");
shouldBeFalse("({foo : 'yum'}).hasOwnProperty('toString')");
shouldBeFalse("''.hasOwnProperty('toString')");

var successfullyParsed = true;

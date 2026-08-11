description(
"This test for a crash when optimizing expressions of the form 'typeof o == constant' where 'constant' is not a string."
);

var o = { };

shouldBeFalse("typeof o == undefined");
shouldBeFalse("typeof o == null");
shouldBeFalse("typeof o == true");
shouldBeFalse("typeof o == false");
shouldBeFalse("typeof o == 1");
shouldBeFalse("typeof o == 1.0");
shouldBeFalse("typeof o == { }");

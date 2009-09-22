description(
"This test checks that functions re-string-ify in a way that is syntactically " +
"compatible with concatenation."
);

shouldBe("(function(){return}).toString()", "'function () {return;}'");
shouldBe("(function(){return }).toString()", "'function () {return; }'");
shouldBe("(function(){return" + "\n" + "}).toString()", "'function () {return;" + "\\n" + "}'");
shouldBe("(function(){}).toString()", "'function () {}'");
shouldBe("(function(){ }).toString()", "'function () { }'");

var successfullyParsed = true;

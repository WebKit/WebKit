description(
'This test checks that setting a non-object, non-null value for __proto__ does not lead to a crash when next setting a property on the object.  This was reported as <a href="http://bugs.webkit.org/show_bug.cgi?id=17925">bug 17925</a>.'
);

x = {};
originalProto = x.__proto__;
x.__proto__ = 1;
shouldBe("x.__proto__", "originalProto");

x.someProperty = 1;
debug('If we got to this point then we did not crash and the test has passed.');
var successfullyParsed = true;

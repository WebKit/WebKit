description(
'This test checks that setting a non-object, non-null value for this.__proto__ does not lead to a crash when setting a property on the this object.'
);

originalProto = this.__proto__;
this.__proto__ = 1;

shouldBe("this.__proto__", "originalProto");

someProperty = 1;
debug('If we got to this point then we did not crash and the test has passed.');
var successfullyParsed = true;

description(
"This tests that we can correctly call Function.prototype.call"
);

var myObject = { call: function() { return [myObject, "myObject.call"] } };
var myFunction = function (arg1) { return [this, "myFunction", arg1] };
var myFunctionWithCall = function (arg1) { return [this, "myFunctionWithCall", arg1] };
myFunctionWithCall.call = function (arg1) { return [this, "myFunctionWithCall.call", arg1] };
Function.prototype.aliasedCall = Function.prototype.call;

shouldBe("myObject.call()", '[myObject, "myObject.call"]');
shouldBe("myFunction('arg1')", '[this, "myFunction", "arg1"]');
shouldBe("myFunction.call(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
shouldBe("myFunction.call()", '[this, "myFunction", undefined]');
shouldBe("myFunction.call(null)", '[this, "myFunction", undefined]');
shouldBe("myFunction.call(undefined)", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedCall(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
shouldBe("myFunction.aliasedCall()", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedCall(null)", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedCall(undefined)", '[this, "myFunction", undefined]');
shouldBe("myFunctionWithCall.call(myObject, 'arg1')", '[myFunctionWithCall, "myFunctionWithCall.call", myObject]');
shouldBe("myFunctionWithCall.aliasedCall(myObject, 'arg1')", '[myObject, "myFunctionWithCall", "arg1"]');

var successfullyParsed = true;

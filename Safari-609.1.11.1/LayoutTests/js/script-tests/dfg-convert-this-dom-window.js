description(
"This tests that we can correctly call Function.prototype.call in the DFG, but more precisely, that we give the correct this object in case it is undefined"
);

var myObject = { call: function() { return [myObject, "myObject.call"] } };
var myFunction = function (arg1) { return [this, "myFunction", arg1] };
var myFunctionWithCall = function (arg1) { return [this, "myFunctionWithCall", arg1] };
myFunctionWithCall.call = function (arg1) { return [this, "myFunctionWithCall.call", arg1] };
Function.prototype.aliasedCall = Function.prototype.call;

dfgShouldBe(myObject.call, "myObject.call()", '[myObject, "myObject.call"]');
dfgShouldBe(myFunction, "myFunction('arg1')", '[this, "myFunction", "arg1"]');
dfgShouldBe(myFunction, "myFunction.call(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
dfgShouldBe(myFunction, "myFunction.call()", '[this, "myFunction", undefined]');
dfgShouldBe(myFunction, "myFunction.call(null)", '[this, "myFunction", undefined]');
dfgShouldBe(myFunction, "myFunction.call(undefined)", '[this, "myFunction", undefined]');
dfgShouldBe(myFunction, "myFunction.aliasedCall(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
dfgShouldBe(myFunction, "myFunction.aliasedCall()", '[this, "myFunction", undefined]');
dfgShouldBe(myFunction, "myFunction.aliasedCall(null)", '[this, "myFunction", undefined]');
dfgShouldBe(myFunction, "myFunction.aliasedCall(undefined)", '[this, "myFunction", undefined]');
dfgShouldBe(myFunctionWithCall.call, "myFunctionWithCall.call(myObject, 'arg1')", '[myFunctionWithCall, "myFunctionWithCall.call", myObject]');
dfgShouldBe(myFunctionWithCall, "myFunctionWithCall.aliasedCall(myObject, 'arg1')", '[myObject, "myFunctionWithCall", "arg1"]');


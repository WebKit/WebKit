description('Test caller and arguments properties in function in non strict mode');

function foo() {
    return 1;
}

shouldBe('Object.getOwnPropertyNames(function () {}).length','5');

shouldBeTrue('Object.getOwnPropertyNames(function () {}).includes("caller")');
shouldBeTrue('Object.getOwnPropertyNames(function () {}).includes("arguments")');

shouldBeTrue('(function(){}).hasOwnProperty("caller")');
shouldBeTrue('(function(){}).__proto__.hasOwnProperty("caller")');

shouldBeTrue('(function(){}).hasOwnProperty("arguments")');
shouldBeTrue('(function(){}).__proto__.hasOwnProperty("arguments")');

shouldBe('typeof Object.getOwnPropertyDescriptor(foo, "arguments")', '"object"');
shouldBe('typeof Object.getOwnPropertyDescriptor(foo, "caller")', '"object"');

shouldBe('foo.caller', 'null');
shouldBe('foo.arguments', 'null');

foo.caller = 10;
foo.arguments = 10;

shouldBe('foo.caller', 'null');
shouldBe('foo.arguments', 'null');

var boo = function () { return boo.arguments; };

shouldBe('boo("abc")[0]','"abc"');

boo.arguments = 'not-expected-value';
shouldBe('boo("expected-value")[0]','"expected-value"');

var f = function () { return f.caller;  };
var g = function (cb) { return cb(); };

shouldBe('g(f)','g');

var doSetCaller = function (value, doDelete) {
	var f = function () {};
	if (doDelete)
		delete f.__proto__.caller;
	f.__proto__.caller = value;
	return f;
};

var value = "property-value";

shouldThrow("doSetCaller(value, false)", "'TypeError: \\'arguments\\', \\'callee\\', and \\'caller\\' cannot be accessed in this context.'");
shouldBe("doSetCaller(value, true).__proto__.caller", "value");


var doSetArguments = function (value, doDelete) {
	var f = function () {};
	if (doDelete)
		delete f.__proto__.arguments;
	f.__proto__.arguments = value;
	return f;
};

shouldThrow("doSetArguments(value, false)", "'TypeError: \\'arguments\\', \\'callee\\', and \\'caller\\' cannot be accessed in this context.'");
shouldBe("doSetArguments(value, true).__proto__.arguments", "value");

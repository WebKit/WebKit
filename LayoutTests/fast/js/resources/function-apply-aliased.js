description(
"This tests that we can correctly call Function.prototype.apply"
);

var myObject = { apply: function() { return [myObject, "myObject.apply"] } };
var myFunction = function (arg1) {
    return [this, "myFunction", arg1];
};
var myFunctionWithApply = function (arg1) { 
    return [this, "myFunctionWithApply", arg1];
};

function forwarder(f, thisValue, args) {
    function g() {
        return f.apply(thisValue, arguments);
    }
    return g.apply(null, args);
}
function recurseArguments() {
    recurseArguments.apply(null, arguments);
}

myFunctionWithApply.apply = function (arg1) { return [this, "myFunctionWithApply.apply", arg1] };
Function.prototype.aliasedApply = Function.prototype.apply;
var arg1Array = ['arg1'];

shouldBe("myObject.apply()", '[myObject, "myObject.apply"]');
shouldBe("forwarder(myObject)", '[myObject, "myObject.apply"]');
shouldBe("myFunction('arg1')", '[this, "myFunction", "arg1"]');
shouldBe("forwarder(myFunction, null, ['arg1'])", '[this, "myFunction", "arg1"]');
shouldBe("myFunction.apply(myObject, ['arg1'])", '[myObject, "myFunction", "arg1"]');
shouldBe("myFunction.apply(myObject, arg1Array)", '[myObject, "myFunction", "arg1"]');
shouldBe("forwarder(myFunction, myObject, arg1Array)", '[myObject, "myFunction", "arg1"]');
shouldBe("myFunction.apply()", '[this, "myFunction", undefined]');
shouldBe("myFunction.apply(null)", '[this, "myFunction", undefined]');
shouldBe("myFunction.apply(undefined)", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedApply(myObject, ['arg1'])", '[myObject, "myFunction", "arg1"]');
shouldBe("myFunction.aliasedApply()", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedApply(null)", '[this, "myFunction", undefined]');
shouldBe("myFunction.aliasedApply(undefined)", '[this, "myFunction", undefined]');
shouldBe("myFunctionWithApply.apply(myObject, ['arg1'])", '[myFunctionWithApply, "myFunctionWithApply.apply", myObject]');
shouldBe("myFunctionWithApply.aliasedApply(myObject, ['arg1'])", '[myObject, "myFunctionWithApply", "arg1"]');
shouldBe("myFunctionWithApply.apply(myObject, arg1Array)", '[myFunctionWithApply, "myFunctionWithApply.apply", myObject]');
shouldBe("forwarder(myFunctionWithApply, myObject, arg1Array)", '[myFunctionWithApply, "myFunctionWithApply.apply", myObject]');
shouldBe("myFunctionWithApply.aliasedApply(myObject, arg1Array)", '[myObject, "myFunctionWithApply", "arg1"]');

// Blow the stack with a sparse array
shouldThrow("myFunction.apply(null, new Array(5000000))");
// Blow the stack with a sparse array that is sufficiently large to cause int overflow
shouldThrow("myFunction.apply(null, new Array(1 << 30))");
// Blow the stack recursing with arguments
shouldThrow("recurseArguments.apply(null, new Array(50000))");

var successfullyParsed = true;

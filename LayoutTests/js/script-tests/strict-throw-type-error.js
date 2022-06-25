description("ThrowTypeError is a singleton object");

function getter(object, name)
{
    Object.getOwnPropertyDescriptor(object, name).get;
}

function strictArgumentsFunction1()
{
    "use strict";
    return arguments;
}
var strictArguments1 = strictArgumentsFunction1();
var boundFunction1 = strictArgumentsFunction1.bind();
var functionCaller1 = getter(strictArgumentsFunction1.__proto__, "caller");
var functionArguments1 = getter(strictArgumentsFunction1.__proto__, "arguments");
var argumentsCaller1 = Object.getOwnPropertyDescriptor(strictArguments1, "caller");
var argumentsCallee1 = getter(strictArguments1, "callee");
var boundCaller1 = Object.getOwnPropertyDescriptor(boundFunction1, "caller");
var boundArguments1 = Object.getOwnPropertyDescriptor(boundFunction1, "arguments");

function strictArgumentsFunction2()
{
    "use strict";
    return arguments;
}
var strictArguments2 = strictArgumentsFunction2();
var boundFunction2 = strictArgumentsFunction2.bind();
var functionCaller2 = getter(strictArgumentsFunction2.__proto__, "caller");
var functionArguments2 = getter(strictArgumentsFunction2.__proto__, "arguments");
var argumentsCaller2 = Object.getOwnPropertyDescriptor(strictArguments2, "caller");
var argumentsCallee2 = getter(strictArguments2, "callee");
var boundCaller2 = Object.getOwnPropertyDescriptor(boundFunction2, "caller");
var boundArguments2 = Object.getOwnPropertyDescriptor(boundFunction2, "arguments");

shouldBeTrue('functionCaller1 === functionCaller2');

shouldBeTrue('functionCaller1 === functionArguments1');
shouldBe('argumentsCaller1', 'undefined');
shouldBeTrue('functionCaller1 === argumentsCallee1');
shouldBe('boundCaller1', 'undefined');
shouldBe('boundArguments1', 'undefined');

shouldBeTrue('functionCaller2 === functionArguments2');
shouldBe('argumentsCaller2', 'undefined');
shouldBeTrue('functionCaller2 === argumentsCallee2');
shouldBe('boundCaller2', 'undefined');
shouldBe('boundArguments2', 'undefined');

successfullyParsed = true;

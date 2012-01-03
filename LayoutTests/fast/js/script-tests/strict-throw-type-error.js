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
var functionCaller1 = getter(strictArgumentsFunction1, "caller");
var functionArguments1 = getter(strictArgumentsFunction1, "arguments");
var argumentsCaller1 = getter(strictArguments1, "caller");
var argumentsCallee1 = getter(strictArguments1, "callee");
var boundCaller1 = getter(boundFunction1, "caller");
var boundArguments1 = getter(boundFunction1, "arguments");

function strictArgumentsFunction2()
{
    "use strict";
    return arguments;
}
var strictArguments2 = strictArgumentsFunction2();
var boundFunction2 = strictArgumentsFunction2.bind();
var functionCaller2 = getter(strictArgumentsFunction2, "caller");
var functionArguments2 = getter(strictArgumentsFunction2, "arguments");
var argumentsCaller2 = getter(strictArguments2, "caller");
var argumentsCallee2 = getter(strictArguments2, "callee");
var boundCaller2 = getter(boundFunction2, "caller");
var boundArguments2 = getter(boundFunction2, "arguments");

shouldBeTrue('functionCaller1 === functionCaller2');

shouldBeTrue('functionCaller1 === functionArguments1');
shouldBeTrue('functionCaller1 === argumentsCaller1');
shouldBeTrue('functionCaller1 === argumentsCallee1');
shouldBeTrue('functionCaller1 === boundCaller1');
shouldBeTrue('functionCaller1 === boundArguments1');

shouldBeTrue('functionCaller2 === functionArguments2');
shouldBeTrue('functionCaller2 === argumentsCaller2');
shouldBeTrue('functionCaller2 === argumentsCallee2');
shouldBeTrue('functionCaller2 === boundCaller2');
shouldBeTrue('functionCaller2 === boundArguments2');

successfullyParsed = true;

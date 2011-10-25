description(
"This test checks that constructor properties are not enumeratable, but are writable deletable."
);

function canEnum(object, propertyName)
{
    for (prop in object)
        if (prop == propertyName)
            return true;
    return false;
}

function checkConstructor(expression)
{
    shouldBe(expression + ".hasOwnProperty('constructor')", "true");
    shouldBe("canEnum(" + expression + ", 'constructor')", "false");
    shouldBe("x = " + expression + "; x.constructor = 4; x.constructor", "4");
    shouldBe("x = " + expression + "; delete x.constructor; x.hasOwnProperty('constructor')", "false");
}

checkConstructor("(function () { }).prototype");
function declaredFunction() { }
checkConstructor("declaredFunction.prototype");
checkConstructor("(new Function).prototype");

checkConstructor("Array.prototype");
checkConstructor("Boolean.prototype");
checkConstructor("Date.prototype");
checkConstructor("Error.prototype");
checkConstructor("EvalError.prototype");
checkConstructor("Function.prototype");
checkConstructor("Number.prototype");
checkConstructor("Object.prototype");
checkConstructor("RangeError.prototype");
checkConstructor("ReferenceError.prototype");
checkConstructor("RegExp.prototype");
checkConstructor("String.prototype");
checkConstructor("SyntaxError.prototype");
checkConstructor("TypeError.prototype");
checkConstructor("URIError.prototype");

checkConstructor("document.createTextNode('')");

description(
"This test checks the names of all sorts of different functions."
);

document.documentElement.setAttribute("onclick", " return 2; ");

shouldBe("new Function(' return 1; ').toString().replace(/[ \\n]+/g, ' ')", "'function anonymous() { return 1; }'");
shouldBe("document.documentElement.onclick.toString().replace(/[ \\n]+/g, ' ')", "'function onclick(event) { return 2; }'");

shouldBe("''.constructor", "String");

function checkConstructorName(name)
{
    shouldBe(name + ".toString()", "'function " + name + "() {\\n    [native code]\\n}'");
}

checkConstructorName("Boolean");
checkConstructorName("Date");
checkConstructorName("Error");
checkConstructorName("EvalError");
checkConstructorName("Function");
checkConstructorName("Number");
checkConstructorName("Object");
checkConstructorName("RangeError");
checkConstructorName("ReferenceError");
checkConstructorName("RegExp");
checkConstructorName("String");
checkConstructorName("SyntaxError");
checkConstructorName("TypeError");
checkConstructorName("URIError");

var successfullyParsed = true;

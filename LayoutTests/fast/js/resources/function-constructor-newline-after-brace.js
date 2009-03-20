description(
"This test checks that the Function constructor places a newline after the opening brace, for compatibility reasons. It passes if there are no syntax error exceptions."
);

function addToFunction(oldFunction, name)
{
    var tempString = "";
    if (oldFunction != null)  {
        tempString = oldFunction.toString();
        var index = tempString.indexOf("{") + 2;
        tempString = tempString.substr(index, tempString.length - index - 2);
    }
    return new Function(name + '_obj.initialize();' + tempString);
}

var f1 = addToFunction(null, "f1");
addToFunction(f1, "f2");

function addToFunctionWithArgument(oldFunction, name)
{
    var tempString = "";
    if (oldFunction != null)  {
        tempString = oldFunction.toString();
        var index = tempString.indexOf("{") + 2;
        tempString = tempString.substr(index, tempString.length - index - 2);
    }
    return new Function("arg", name + '_obj.initialize();' + tempString);    
}

var g1 = addToFunctionWithArgument(null, "g1");
addToFunctionWithArgument(g1, "g2");

var successfullyParsed = true;

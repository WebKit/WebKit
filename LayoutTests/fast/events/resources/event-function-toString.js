description("Test serialization of event handlers for bug 25544.");

function normalizedFunctionString(string)
{
    // Remove the white space before/after { } which differs between browsers
    string = string.replace(/\{\s+/g, "{");
    string = string.replace(/\s+\}/g, "}");
    return string;
}

var div = document.createElement("div");
div.setAttribute("onclick", "test();");
shouldBeEqualToString("normalizedFunctionString(div.onclick.toString())", "function onclick(event) {test();}");

var path = document.createElementNS("http://www.w3.org/2000/svg", "path");
path.setAttribute("onclick", "test();");
shouldBeEqualToString("normalizedFunctionString(path.onclick.toString())", "function onclick(evt) {test();}");

var successfullyParsed = true;

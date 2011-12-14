description("Test to ensure that js constructors create objects with the correct constructor");

var otherGlobal = document.getElementsByTagName("iframe")[0].contentWindow;
var constructors = ["Object", "Function", "Array", "Number", "String", "Boolean", "RegExp", "Date", "Error", "RangeError", "ReferenceError", "SyntaxError", "TypeError", "URIError", "Image"];

for (var i = 0; i < constructors.length; i++) {
    shouldBeTrue("new otherGlobal." + constructors[i] + "() instanceof otherGlobal." + constructors[i]);
    try {
        if ((typeof (otherGlobal[constructors[i]]())) === "object" || (typeof (otherGlobal[constructors[i]]())) === "function")
            shouldBeTrue("otherGlobal." + constructors[i] + "() instanceof otherGlobal." + constructors[i]);
    } catch(e) {
    
    }
}

successfullyParsed = true;

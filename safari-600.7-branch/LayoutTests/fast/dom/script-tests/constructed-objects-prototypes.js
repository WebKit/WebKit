description("Make sure prototypes are set up using the window a property came from, instead of the lexical global object.")

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);
var inner = subframe.contentWindow; // Call it "inner" to make shouldBe output shorter

// Stash a property on the prototypes.
window.Object.prototype.isInner = false;
inner.Object.prototype.isInner = true;

var constructorNames = ["Image", "MediaController", "Option", "OverflowEvent", "ProgressEvent", "URL", "XMLHttpRequest"];

var argumentsForConstructor = {
    'URL' : "'about:blank'",
    'Worker' : "'foo'",
}

for (var x = 0; x < constructorNames.length; x++) {
    var constructorName = constructorNames[x];
    var arguments = argumentsForConstructor[constructorName] || "";
    var argumentsString = "(" + arguments + ")";
    // Test first to see if the object is constructable
    var constructedObject;
    try {
        constructedObject = eval("new inner." + constructorName + argumentsString);
    } catch(e) {
        continue;
    }

    shouldBeTrue("(new inner." + constructorName + argumentsString + ").isInner");
    shouldBeTrue("(new inner." + constructorName + argumentsString + ").constructor.isInner");
}

document.body.removeChild(subframe);

description("Make sure prototypes are set up using the window a property came from, instead of the lexical global object.")

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);
var inner = subframe.contentWindow; // Call it "inner" to make shouldBe output shorter

// Stash a property on the prototypes.
window.Object.prototype.isInner = false;
inner.Object.prototype.isInner = true;

function classNameForObject(object)
{
    // call will use the global object if passed null or undefined, so special case those:
    if (object == null)
        return null;
    var result = Object.prototype.toString.call(object);
    // remove '[object ' and ']'
    return result.split(' ')[1].split(']')[0];
}

function constructorPropertiesOnWindow(globalObject)
{
    var constructorNames = [];
    for (var property in globalObject) {
        var value = inner[property];
        if (value == null)
            continue;
        var type = classNameForObject(value);
        // Ignore these properties because they do not exist in all implementations. They will be tested separately
        if (type == "WebGLRenderingContextConstructor" ||
            type == "ArrayBufferConstructor" ||
            type =="Float32ArrayConstructor" ||
            type =="Float64ArrayConstructor" ||
            type =="Int8ArrayConstructor" ||
            type =="Int16ArrayConstructor" ||
            type =="Int32ArrayConstructor" ||
            type =="Uint8ArrayConstructor" ||
            type =="Uint8ClampedArrayConstructor" ||
            type =="Uint16ArrayConstructor" ||
            type =="Uint32ArrayConstructor" ||
            type == "FileErrorConstructor" ||
            type == "FileReaderConstructor" ||
            type == "WebKitBlobBuilderConstructor" ||
            type == "AudioContextConstructor")
            continue;
        if (!type.match('Constructor$'))
            continue;
        constructorNames.push(property);
    }
    return constructorNames.sort();
}

var constructorNames = constructorPropertiesOnWindow(inner);

var argumentsForConstructor = {
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

if (this.importScripts)
    importScripts('../resources/js-test-pre.js');

function descriptorShouldBe(object, property, expected) {
    var test = "Object.getOwnPropertyDescriptor(" + object + ", '" + eval(property) + "')";
    if ("writable" in expected) {
        shouldBe(test + ".value", "" + expected.value);
        shouldBeFalse(test + ".hasOwnProperty('get')");
        shouldBeFalse(test + ".hasOwnProperty('set')");
    } else {
        shouldBe(test + ".get", "" + expected.get);
        shouldBe(test + ".set", "" + expected.set);
        shouldBeFalse(test + ".hasOwnProperty('value')");
        shouldBeFalse(test + ".hasOwnProperty('writable')");
    }
    shouldBe(test + ".enumerable", "" + expected.enumerable);
    shouldBe(test + ".configurable", "" + expected.configurable);
}

function classNameForObject(object)
{
    // call will use the global object if passed null or undefined, so special case those:
    if (object == null)
        return null;
    var result = Object.prototype.toString.call(object);
    // remove "[object " and "]"
    return result.split(" ")[1].split("]")[0];
}

function constructorPropertiesOnGlobalObject(globalObject)
{
    var constructorNames = [];
    var propertyNames = Object.getOwnPropertyNames(global);
    for (var i = 0; i < propertyNames.length; i++) {
        var value = global[propertyNames[i]];
        if (value == null)
            continue;
        var type = classNameForObject(value);
        if (!type.match("Constructor$") || propertyNames[i] == "constructor")
            continue;
        constructorNames.push(propertyNames[i]);
    }
    return constructorNames.sort();
}

var global = this;
var constructorNames = constructorPropertiesOnGlobalObject(global);

var constructorName;
for (var i = 0; i < constructorNames.length; i++) {
    constructorName = constructorNames[i];
    descriptorShouldBe("global", "constructorName", {writable: true, enumerable: false, configurable: true, value: constructorName});
}

if (isWorker())
  finishJSTest();

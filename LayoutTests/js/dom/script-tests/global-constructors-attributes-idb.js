if (self.importScripts) {
     importScripts('../../../resources/js-test-pre.js');

    if (!self.postMessage) {
        // Shared worker.  Make postMessage send to the newest client, which in
        // our tests is the only client.

        // Store messages for sending until we have somewhere to send them.
        self.postMessage = function(message) {
            if (typeof self.pendingMessages === "undefined")
                self.pendingMessages = [];
            self.pendingMessages.push(message);
        };
        self.onconnect = function(event) {
            self.postMessage = function(message) {
                event.ports[0].postMessage(message);
            };
            // Offload any stored messages now that someone has connected to us.
            if (typeof self.pendingMessages === "undefined")
                return;
            while (self.pendingMessages.length)
                event.ports[0].postMessage(self.pendingMessages.shift());
        };
    }
}

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
        var descriptor = Object.getOwnPropertyDescriptor(global, propertyNames[i]);
        if (type == "Function" && descriptor.writable && !descriptor.enumerable && descriptor.configurable
            && (propertyNames[i][0].toUpperCase() === propertyNames[i][0] || propertyNames[i].startsWith("webkit")))
            constructorNames.push(propertyNames[i]);
    }
    return constructorNames.sort();
}

var global = this;
var constructorNames = constructorPropertiesOnGlobalObject(global);

var constructorName;
for (var i = 0; i < constructorNames.length; i++) {
    constructorName = constructorNames[i];
    if (constructorName.indexOf("IDB") != 0 && constructorName.indexOf("webkitIDB") != 0)
        continue;
    descriptorShouldBe("global", "constructorName", {writable: true, enumerable: false, configurable: true, value: constructorName});
}

if (isWorker())
  finishJSTest();

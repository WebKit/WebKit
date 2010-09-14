description("Make sure prototypes are set up using the window a property came from, instead of the lexical global object.")

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);
var inner = subframe.contentWindow; // Call it "inner" to make shouldBe output shorter

var skippedProperties = [
    // These reach outside the frame:
    "parent", "top", "opener",
    // These are defined by DumpRenderTree
    "GCController", "layoutTestController",
    "objCController", "textInputController", "navigationController",
    "eventSender", "objCPlugin", "objCPluginFunction",
    "appleScriptController", "plainText", "accessibilityController",
    // Ignore these properties because they do not exist in all implementations. They will be tested separately
    "webkitNotifications",
    "webkitPerformance",
    "WebGLRenderingContext",
    "ArrayBuffer",
    "Int8Array", "Uint8Array", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Float32Array",
    "FileError", "FileReader", "requestFileSystem",
    "indexedDB", "IDBKeyRange", "IDBCursor", "IDBDatabase", "IDBDatabaseError", "IDBDatabaseException", "IDBErrorEvent", "IDBEvent", "IDBFactory", "IDBIndex", "IDBObjectStore", "IDBRequest", "IDBSuccessEvent", "IDBTransaction",
    "showModalDialog",
    "createBlobURL", "revokeBlobURL",
    "DeviceOrientationEvent",
    "TEMPORARY", "PERSISTENT",
    "Flags",
    // Ignore this property because it only appears in debug builds.
    "jscprint"
];

var skippedPropertiesSet = {};
for (var i = 0; i < skippedProperties.length; i++)
  skippedPropertiesSet[skippedProperties[i]] = true;

// Stash a property on the prototypes.
window.Object.prototype.isInner = false;
inner.Object.prototype.isInner = true;

function propertiesOnObject(o) {
    var namesSet = {};
    while (o && typeof o == "object") {
        var names = Object.getOwnPropertyNames(o);
        for (var i = 0; i < names.length; ++i)
            namesSet[names[i]] = 1;
        o = Object.getPrototypeOf(o);
    }
    var result = [];
    for (var p in namesSet)
        result.push(p);
    return result;
}

windowProperites = propertiesOnObject(window).sort();

for (var x = 0; x < windowProperites.length; x++) {
    var property = windowProperites[x];
    if (skippedPropertiesSet[property])
        continue;

    var value = inner[property];
    // Skip functions/properties on window which won't be on inner (like the ones defined in this test)
    if (value === undefined)
        continue;
    // Skip enumerable properties which default to null (like on* listeners)
    if (value === null)
        continue;
    shouldBeTrue("inner." + property + ".isInner");
    shouldBeTrue("inner." + property + ".constructor.isInner");
}

document.body.removeChild(subframe);

var successfullyParsed = true;

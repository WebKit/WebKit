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
];

var skippedPropertiesSet = {};
for (var i = 0; i < skippedProperties.length; i++)
  skippedPropertiesSet[skippedProperties[i]] = true;

// Stash a property on the prototypes.
window.Object.prototype.isInner = false;
inner.Object.prototype.isInner = true;

var windowProperites = [];
for (property in window) {
    windowProperites.push(property);
}
windowProperties = windowProperites.sort();

for (var x = 0; x < windowProperites.length; x++) {
    var property = windowProperites[x];
    if (skippedPropertiesSet[property])
        continue;

    // Skip null properties (like on* listeners)
    var value = inner[property];
    if (!value)
        continue;

    shouldBeTrue("inner." + property + ".isInner")
}

document.body.removeChild(subframe);

var successfullyParsed = true;

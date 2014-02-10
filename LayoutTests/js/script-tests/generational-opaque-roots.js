description("Tests that opaque roots behave correctly during young generation collections.");

// Create the primary Root.
var root = new Root();
// This secondary root is for allocating a second Element without overriding 
// the primary Root's Element.
var otherRoot = new Root();

// Run an Eden collection so that the Root will be in the old gen (and won't be rescanned).
edenGC();

// Create a new Element and set a custom property on it.
var elem = new Element(root);
elem.customProperty = "hello";

// Make the Element unreachable except through the ephemeron with the Root.
elem = null;

// Create another Element so that we process the weak handles in block of the original Element.
var test = new Element(otherRoot);

// Run another Eden collection to process the weak handles in the Element's block. If opaque roots
// are cleared then we'll think that the original Element is dead because the Root won't be in the 
// set of opaque roots.
edenGC();

// Check if the primary Root's Element exists and has our custom property.
var elem = getElement(root);
shouldBe("elem.customProperty", "\"hello\"");

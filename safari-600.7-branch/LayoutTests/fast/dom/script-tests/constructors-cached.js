description("This test ensures that objects with security restrictions are cached correctly");

var ctors = ["Image", "Option", "XMLHttpRequest", "Audio"];

for (var i = 0; i < ctors.length; i++) {
    var ctor = ctors[i];
    try {
        // Test retrieving the object twice results in the same object
        shouldBe(ctor, ctor);

        // Be paranoid -- make sure that setting a property results in that property
        // stays
        this[ctor].testProperty = "property set successfully";
        shouldBe(ctor + ".testProperty", '"property set successfully"');
    } catch (e) {
        testFailed("Testing " + ctor + " threw " + e);
    }
}

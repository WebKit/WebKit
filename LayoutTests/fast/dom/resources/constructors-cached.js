description("This test ensures that objects with security restrictions are cached correctly");

var constructors = ["Image", "Option", "XMLHttpRequest", "Audio"];

for (var i = 0; i < constructors.length; i++) {
    var constructor = constructors[i];
    try {
        // Test retrieving the object twice results in the same object
        shouldBe(constructor, constructor);

        // Be paranoid -- make sure that setting a property results in that property
        // stays
        this[constructor].testProperty = "property set successfully";
        shouldBe(constructor + ".testProperty", '"property set successfully"');
    } catch (e) {
        testFailed("Testing " + constructor + " threw " + e);
    }
}

successfullyParsed = true;

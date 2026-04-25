description(
"Tests that putting an object into dictionary mode when it has a blank indexing type doesn't cause us to crash."
);

// Freezing the Array prototype causes us to create ArrayStorage on the prototype, which has a blank indexing type.
Object.freeze(Array.prototype);

shouldBe("Object.isFrozen(Array.prototype)", "true");

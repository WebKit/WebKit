description(
'If an object has an indexed property shadowing a property of the same name on the prototype, the correct shadowing property should be returned - even if teh property from the prototype comes from an overriden implementation of getOwPropertySlot.'
);

function test(x) {
    var testObject = {
        __proto__: new String("X"),
        "0": "success"
    };
    return testObject[x];
}

shouldBe('test("0")', '"success"');

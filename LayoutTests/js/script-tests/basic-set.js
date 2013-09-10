description("Tests basic correctness of ES Set object");

// Currently we don't support iterators, so we throw
// on any non-throwing parameters
shouldBeFalse("Set instanceof Set");
shouldBeFalse("Set.prototype instanceof Set");
shouldBeTrue("Set() instanceof Set");
shouldBeTrue("new Set() instanceof Set");
shouldBeTrue("Set(null) instanceof Set");
shouldBeTrue("Set(undefined) instanceof Set");
shouldBeTrue("Set(undefined, undefined) instanceof Set");
shouldBeTrue("Set(null, undefined) instanceof Set");

shouldThrow("Set(1)");
shouldThrow("Set(true)");
shouldThrow("Set([])");
shouldThrow("Set({})");
shouldThrow("Set(undefined, null)");
shouldThrow("Set(undefined, {})");

// Basic test for constructor
var set = new Set(1, undefined, true, 6, true, "1", 0, {})
set.forEach(debug);

var set = new Set;
shouldBeFalse("Object.hasOwnProperty(set, 'size')")
shouldBeTrue("Set.prototype.hasOwnProperty('size')")
shouldThrow("Map.prototype.size")
shouldBe("set.size", "0")
shouldBe("set.add(-0)", "set")
shouldBe("set.add(0)", "set")
shouldBe("set.add(Infinity)", "set")
shouldBe("set.add(-Infinity)", "set")
shouldBe("set.add(NaN)", "set")
shouldBe("set.add('0')", "set")
shouldBe("set.add(0.1)", "set")
shouldBe("set.size", "7")

shouldBeTrue("set.has(-0)")
shouldBeTrue("set.has(0)")
shouldBeTrue("set.has(Infinity)")
shouldBeTrue("set.has(-Infinity)")
shouldBeTrue("set.has(NaN)")
shouldBeTrue("set.has('0')")
shouldBeTrue("set.has(0.1)")

shouldBeTrue("set.delete(-0)")
shouldBeTrue("set.delete(0)")
shouldBeTrue("set.delete(Infinity)")
shouldBeTrue("set.delete(-Infinity)")
shouldBeTrue("set.delete(NaN)")
shouldBeTrue("set.delete('0')")
shouldBeTrue("set.delete(0.1)")

shouldBeFalse("set.delete(-0)")
shouldBeFalse("set.delete(0)")
shouldBeFalse("set.delete(Infinity)")
shouldBeFalse("set.delete(-Infinity)")
shouldBeFalse("set.delete(NaN)")
shouldBeFalse("set.delete('0')")
shouldBeFalse("set.delete(0.1)")

var simpleString = "aaaaa";
var otherString = "";
for (var i = 0; i < 5; i++)
	otherString += "a"
set.add(simpleString, {});
shouldBeTrue("set.has(simpleString)")
shouldBeTrue("set.has(otherString)");



shouldBeUndefined("set.clear()");
shouldBe("set.size", "0")
var count = 7;
for (var i = 0; i < count; i++) {
    shouldBe("set.add(" + i + ")", "set")
    shouldBe("set.add('" + i + "')", "set")
}

shouldBe("set.size", "14")

var thisValue = 0xf00
var testThis;
var count = 0;

try {
	set.forEach(function() { "use strict"; debug("forEach #0"); testThis = this; shouldBeUndefined("testThis"); throw count++;  })
} catch(e) {
    debug(e)
}
try {
    set.forEach(function() { "use strict"; debug("forEach #1"); testThis = this; shouldBe("testThis", "thisValue"); throw count++;  }, thisValue)
} catch(e) {
    debug(e)
}

shouldBeUndefined('set.forEach(debug)')

var keys = [];
set.forEach(function (k) {
	debug(k + " : " + typeof (k))

	if (k == 2) {
		set.delete(3)
		set.delete(2)
	} else
	keys.push(k);
	if (k == "3") {
		set.add("3");
		set.delete(4)
		set.add(4)
	}
	gc()
})

gc();
for (var i = 0; i < keys.length; i++) {
	var expectedKey = keys[i]
	shouldBeTrue("set.has("+JSON.stringify(expectedKey)+")");
}

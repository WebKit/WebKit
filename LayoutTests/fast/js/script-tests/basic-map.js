description("Tests basic correctness of ES Map object");

// Currently we don't support iterators, so we throw
// on any non-throwing parameters
shouldBeFalse("Map instanceof Map");
shouldBeFalse("Map.prototype instanceof Map");
shouldBeTrue("new Map() instanceof Map");
shouldBeTrue("new Map(null) instanceof Map");
shouldBeTrue("new Map(undefined) instanceof Map");
shouldBeTrue("new Map(undefined, undefined) instanceof Map");
shouldBeTrue("new Map(null, undefined) instanceof Map");

shouldThrow("new Map(1)");
shouldThrow("new Map(true)");
shouldThrow("new Map([])");
shouldThrow("new Map({})");
shouldThrow("new Map(undefined, null)");
shouldThrow("new Map(undefined, {})");

var map = new Map;
shouldBeFalse("Object.hasOwnProperty(map, 'size')")
shouldBeTrue("Map.prototype.hasOwnProperty('size')")
shouldThrow("Map.prototype.size")
shouldBe("map.size", "0")
shouldBe("map.set(-0, 1)", "map")
shouldBe("map.set(0, 2)", "map")
shouldBe("map.set(Infinity, 3)", "map")
shouldBe("map.set(-Infinity, 4)", "map")
shouldBe("map.set(NaN, 5)", "map")
shouldBe("map.set('0', 6)", "map")
shouldBe("map.set(0.1, 7)", "map")
shouldBe("map.size", "7")
shouldBe("map.get(-0)", "1")
shouldBe("map.get(0)", "2")
shouldBe("map.get(Infinity)", "3")
shouldBe("map.get(-Infinity)", "4")
shouldBe("map.get(NaN)", "5")
shouldBe("map.get('0')", "6")
shouldBe("map.get(0.1)", "7")

shouldBeTrue("map.has(-0)")
shouldBeTrue("map.has(0)")
shouldBeTrue("map.has(Infinity)")
shouldBeTrue("map.has(-Infinity)")
shouldBeTrue("map.has(NaN)")
shouldBeTrue("map.has('0')")
shouldBeTrue("map.has(0.1)")

shouldBeTrue("map.delete(-0)")
shouldBeTrue("map.delete(0)")
shouldBeTrue("map.delete(Infinity)")
shouldBeTrue("map.delete(-Infinity)")
shouldBeTrue("map.delete(NaN)")
shouldBeTrue("map.delete('0')")
shouldBeTrue("map.delete(0.1)")

shouldBeFalse("map.delete(-0)")
shouldBeFalse("map.delete(0)")
shouldBeFalse("map.delete(Infinity)")
shouldBeFalse("map.delete(-Infinity)")
shouldBeFalse("map.delete(NaN)")
shouldBeFalse("map.delete('0')")
shouldBeFalse("map.delete(0.1)")

var simpleString = "aaaaa";
var otherString = "";
for (var i = 0; i < 5; i++)
	otherString += "a"
map.set(simpleString, {});
shouldBe("map.get(simpleString)", "map.get(otherString)");



shouldBeUndefined("map.clear()");
shouldBe("map.size", "0")
var count = 7;
for (var i = 0; i < count; i++) {
    shouldBe("map.set(" + i + ", " + (i * 2) + ")", "map")
    shouldBe("map.set('" + i + "', " + (i * 2 + 1) + ")", "map")
}

shouldBe("map.size", "14")

var thisValue = 0xf00
var testThis;
var count = 0;

try {
	map.forEach(function() { "use strict"; debug("forEach #0"); testThis = this; shouldBeUndefined("testThis"); throw count++;  })
} catch(e) {
    debug(e)
}
try {
    map.forEach(function() { "use strict"; debug("forEach #1"); testThis = this; shouldBe("testThis", "thisValue"); throw count++;  }, thisValue)
} catch(e) {
    debug(e)
}

shouldBeUndefined('map.forEach(debug)')

map.forEach(function (v,k) {
	debug(k + " : " + typeof (k) + " => " + v)

	if (k == 2)
		map.delete(3)
	if (k == "3") {
		map.set("3", "replaced");
		map.delete(4)
		map.set(4, 11)
	}
})

var keys;
map.forEach(function (v,k) {
	keys = keys || [];
	keys.push([k,v]);
})
gc();
for (var i = 0; i < keys.length; i++) {
	var expectedKey = keys[i][0]
	var expectedValue = keys[i][1]
	shouldBe("map.get("+JSON.stringify(expectedKey)+")", JSON.stringify(expectedValue));
}

description("Tests basic correctness of ES WeakMap object");

// Currently we don't support iterators, so we throw
// on any non-throwing parameters
shouldBeFalse("WeakMap instanceof WeakMap");
shouldBeFalse("WeakMap.prototype instanceof WeakMap");
shouldBeTrue("new WeakMap() instanceof WeakMap");

shouldThrow("WeakMap()");

var map = new WeakMap;
shouldThrow("map.set(0, 1)")
shouldThrow("map.set(0.5, 1)")
shouldThrow("map.set('foo', 1)")
shouldThrow("map.set(true, 1)")
shouldThrow("map.set(false, 1)")
shouldThrow("map.set(null, 1)")
shouldThrow("map.set(undefined, 1)")
shouldThrow("map.get(0)")
shouldThrow("map.get(0.5)")
shouldThrow("map.get('foo')")
shouldThrow("map.get(true)")
shouldThrow("map.get(false)")
shouldThrow("map.get(null)")
shouldThrow("map.get(undefined)")
shouldThrow("map.has(0)")
shouldThrow("map.has(0.5)")
shouldThrow("map.has('foo')")
shouldThrow("map.has(true)")
shouldThrow("map.has(false)")
shouldThrow("map.has(null)")
shouldThrow("map.has(undefined)")
shouldThrow("map.delete(0)")
shouldThrow("map.delete(0.5)")
shouldThrow("map.delete('foo')")
shouldThrow("map.delete(true)")
shouldThrow("map.delete(false)")
shouldThrow("map.delete(null)")
shouldThrow("map.delete(undefined)")

shouldBe("map.set(new String('foo'), 'foo')", "map");
shouldBe("map.get(new String('foo'))", "undefined");
shouldBeFalse("map.has(new String('foo'))");

var keys = [];
var values = [];

var count = 2000;
for (var i = 0; i < count; i++) {
	keys[i] = {}
	values[i] = {v : keys[i]}
	keys[i].v = values[i]
}
for (var i = 0; i < count; i++) {
	map.set(keys[i], values[i])
}
gc();

if (this.GCController) {
	var currentObjectCount = GCController.getJSObjectCount()
    keys = null;
    gc();
    var keyLessObjectCount = GCController.getJSObjectCount()
    // Somewhat approximate
    shouldBeTrue("Math.abs(currentObjectCount - keyLessObjectCount) < 100");
    for (var i = 0; i < count; i++) {
    	if (map.get(values[i].v) != values[i])
    		fail("Key was incorrectly removed from weak map");
    }
    values = null;
    gc();
    var valueLessObjectCount = GCController.getJSObjectCount()
    shouldBeTrue("(keyLessObjectCount - valueLessObjectCount) > " + (count * 3 / 4));
}

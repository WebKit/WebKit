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
shouldBeUndefined("map.get(0)")
shouldBeUndefined("map.get(0.5)")
shouldBeUndefined("map.get('foo')")
shouldBeUndefined("map.get(true)")
shouldBeUndefined("map.get(false)")
shouldBeUndefined("map.get(null)")
shouldBeUndefined("map.get(undefined)")
shouldBeFalse("map.has(0)")
shouldBeFalse("map.has(0.5)")
shouldBeFalse("map.has('foo')")
shouldBeFalse("map.has(true)")
shouldBeFalse("map.has(false)")
shouldBeFalse("map.has(null)")
shouldBeFalse("map.has(undefined)")
shouldBeFalse("map.delete(0)")
shouldBeFalse("map.delete(0.5)")
shouldBeFalse("map.delete('foo')")
shouldBeFalse("map.delete(true)")
shouldBeFalse("map.delete(false)")
shouldBeFalse("map.delete(null)")
shouldBeFalse("map.delete(undefined)")

var object = new String('hello');
shouldBe("map.set(new String('foo'), 'foo')", "map");
shouldBeUndefined("map.get(new String('foo'))");
shouldBeFalse("map.has(new String('foo'))");
shouldBe("map.set(object, 'foo')", "map");
shouldBeTrue("map.has(object)");
shouldBe("map.get(object)", "'foo'");
shouldBeTrue("map.delete(object)");
shouldBeFalse("map.has(object)");
shouldBeFalse("map.delete(object)");
shouldBeUndefined("map.get(object)");

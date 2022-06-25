description("Tests basic correctness of ES WeakSet object");

shouldBeFalse("WeakSet instanceof WeakSet");
shouldBeFalse("WeakSet.prototype instanceof WeakSet");
shouldBeTrue("new WeakSet() instanceof WeakSet");

shouldThrow("WeakSet()");

var set = new WeakSet;
shouldThrow("set.add(0)")
shouldThrow("set.add(0.5)")
shouldThrow("set.add('foo')")
shouldThrow("set.add(true)")
shouldThrow("set.add(false)")
shouldThrow("set.add(null)")
shouldThrow("set.add(undefined)")
shouldThrow("set.add(Symbol.iterator)")
shouldBeFalse("set.has(0)")
shouldBeFalse("set.has(0.5)")
shouldBeFalse("set.has('foo')")
shouldBeFalse("set.has(true)")
shouldBeFalse("set.has(false)")
shouldBeFalse("set.has(null)")
shouldBeFalse("set.has(undefined)")
shouldBeFalse("set.has(Symbol.iterator)")
shouldBeFalse("set.delete(0)")
shouldBeFalse("set.delete(0.5)")
shouldBeFalse("set.delete('foo')")
shouldBeFalse("set.delete(true)")
shouldBeFalse("set.delete(false)")
shouldBeFalse("set.delete(null)")
shouldBeFalse("set.delete(undefined)")
shouldBeFalse("set.delete(Symbol.iterator)")

var object = new String('hello');
shouldBe("set.add(new String('foo'))", "set");
shouldBe("set.add(new String('foo'))", "set");
shouldBeFalse("set.has(new String('foo'))", "false");
shouldBe("set.add(object)", "set");
shouldBeTrue("set.has(object)");
shouldBeFalse("set.delete(new String('foo'))");
shouldBeTrue("set.delete(object)");
shouldBeFalse("set.has(object)");
shouldBeFalse("set.delete(object)");

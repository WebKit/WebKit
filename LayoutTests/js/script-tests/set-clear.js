description("Tests basic correctness of ES Set's clear() API");

// Set containing only String types.
var stringSet = new Set;
stringSet.add('Oliver');
stringSet.add('Benjamin');

stringSet.clear();
shouldBe("stringSet.size", "0");
shouldBe("stringSet.values.length", "0");
shouldBeFalse("stringSet.has('Oliver')");
shouldBeFalse("stringSet.has('Benjamin')");

// Set containing only primitive values.
var valueSet = new Set;
valueSet.add(0);
valueSet.add(1);

valueSet.clear();
shouldBe("valueSet.size", "0");
shouldBe("valueSet.values.length", "0");
shouldBeFalse("valueSet.has(0)");
shouldBeFalse("valueSet.has(1)");

// Set containing objects;
var objectSet = new Set;
var anArray = new Array;
objectSet.add(anArray);
var anObject = new Object;
objectSet.add(anObject);
var otherObject = {"a":1, "b":2};
objectSet.add(otherObject);

objectSet.clear();
shouldBe("objectSet.size", "0");
shouldBe("objectSet.values.length", "0");
shouldBeFalse("objectSet.has(anArray)");
shouldBeFalse("objectSet.has(anObject)");
shouldBeFalse("objectSet.has(otherObject)");

// Mixed types.
var mixedTypeSet = new Set;
mixedTypeSet.add(0);
mixedTypeSet.add('Oliver');
mixedTypeSet.add(stringSet);
mixedTypeSet.add(valueSet);
mixedTypeSet.add(objectSet);
mixedTypeSet.add(anObject);

mixedTypeSet.clear();
shouldBe("mixedTypeSet.size", "0");
shouldBe("mixedTypeSet.values.length", "0");
shouldBeFalse("mixedTypeSet.has(0)");
shouldBeFalse("mixedTypeSet.has('Oliver')");
shouldBeFalse("mixedTypeSet.has(stringSet)");
shouldBeFalse("mixedTypeSet.has(valueSet)");
shouldBeFalse("mixedTypeSet.has(objectSet)");
shouldBeFalse("mixedTypeSet.has(anObject)");

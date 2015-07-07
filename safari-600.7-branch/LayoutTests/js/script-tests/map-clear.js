description("Tests basic correctness of ES Map's clear() API");

// Map containing only String types.
var stringMap = new Map;
stringMap.set('Oliver', 'Hunt');
stringMap.set('Benjamin', 'Poulain');

stringMap.clear();
shouldBe("stringMap.size", "0");
shouldBe("stringMap.values.length", "0");
shouldBeFalse("stringMap.has('Oliver')");
shouldBeFalse("stringMap.has('Benjamin')");

// Map containing only primitive values.
var valueMap = new Map;
valueMap.set(0, 1);
valueMap.set(1, 2);

valueMap.clear();
shouldBe("valueMap.size", "0");
shouldBe("valueMap.values.length", "0");
shouldBeFalse("valueMap.has(0)");
shouldBeFalse("valueMap.has(1)");

// Map containing objects;
var objectMap = new Map;
var anArray = new Array;
objectMap.set(anArray, 0);
var anObject = new Object;
objectMap.set(anObject, 1);
var otherObject = {"a":1, "b":2};
objectMap.set(otherObject, 2);

objectMap.clear();
shouldBe("objectMap.size", "0");
shouldBe("objectMap.values.length", "0");
shouldBeFalse("objectMap.has(anArray)");
shouldBeFalse("objectMap.has(anObject)");
shouldBeFalse("objectMap.has(otherObject)");

// Mixed types.
var mixedTypeMap = new Map;
mixedTypeMap.set(0, objectMap);
mixedTypeMap.set('Oliver', stringMap);
mixedTypeMap.set(stringMap, valueMap);
mixedTypeMap.set(valueMap, anObject);
mixedTypeMap.set(objectMap, objectMap);
mixedTypeMap.set(anObject, stringMap);

mixedTypeMap.clear();
shouldBe("mixedTypeMap.size", "0");
shouldBe("mixedTypeMap.values.length", "0");
shouldBeFalse("mixedTypeMap.has(0)");
shouldBeFalse("mixedTypeMap.has('Oliver')");
shouldBeFalse("mixedTypeMap.has(stringMap)");
shouldBeFalse("mixedTypeMap.has(valueMap)");
shouldBeFalse("mixedTypeMap.has(objectMap)");
shouldBeFalse("mixedTypeMap.has(anObject)");

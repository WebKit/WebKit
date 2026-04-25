description('Test Object.setPrototypeOf.');

function debugEval(str) {
    debug(str);
    eval(str);
}

var value, object, proto, description, protoDescription, oldProto;
var coercibleValueStrings = ["0", "true", "false", "'string'", "Symbol()"];
var nonCoercibleValueStrings = ["undefined", "null"];
var nonNullValueStrings = coercibleValueStrings.concat(["undefined"]);

function getObjectDescriptions() {
    function myFunction() {};
    return [
        { object: myFunction,              description: "Function"  },
        { object: new myFunction(),        description: "Function2" },
        { object: {x: 5},                  description: "Object"    },
        { object: Object.create(null),     description: "Object2"   },
        { object: /regexp/,                description: "RegExp"    },
        { object: [1, 2, 3],               description: "Array"     },
        { object: new Error("test error"), description: "Error"     },
        { object: new Date(),              description: "Date"      },
        { object: new Number(1),           description: "Number"    },
        { object: new Boolean(true),       description: "Boolean"   },
        { object: new String('str'),       description: "String"    },
    ];
}

debug("Basics");
shouldBe("Object.setPrototypeOf.name", "'setPrototypeOf'");
shouldBe("Object.setPrototypeOf.length", "2");

// Coercible objects get coerced.
debug(""); debug("Coercible value");
for (value of coercibleValueStrings) {
    debugEval("value = " + value);
    shouldNotThrow("Object.getPrototypeOf(value)");
    shouldBe("Object.setPrototypeOf(value, {})", "value");
    shouldBe("Object.getPrototypeOf(value)", "(value).__proto__");
}

// Non-coercible object throws a TypeError.
debug(""); debug("Non-Coercible value");
for (value of nonCoercibleValueStrings)
    shouldThrow("Object.setPrototypeOf(" + value + ", {})");

// Non-object/null proto throws a TypeError.
debug(""); debug("Non-Object/Null proto");
for ({object, description} of getObjectDescriptions()) {
    debug("object (" + description + ")");
    for (proto of nonNullValueStrings)
        shouldThrow("Object.setPrototypeOf(object, " + proto + ")");
}

// Object and object proto, more like normal behavior.
debug(""); debug("Object and object proto");
for ({object, description} of getObjectDescriptions()) {
    for ({object: proto, description: protoDescription} of getObjectDescriptions()) {
        debug("object (" + description + ") proto (" + protoDescription + ")");
        shouldBe("Object.setPrototypeOf(object, proto)", "object");
        shouldBe("Object.getPrototypeOf(object)", "proto");
    }
}

// Normal behavior with null proto.
debug(""); debug("Object and null proto");
for ({object, description} of getObjectDescriptions()) {
    debug("object (" + description + ")");
    shouldBe("Object.setPrototypeOf(object, null)", "object");
    shouldBe("Object.getPrototypeOf(object)", "null");
}

// Non-extensible object throws TypeError.
debug(""); debug("Non-extensible object");
for ({object, description} of getObjectDescriptions()) {
    debug("object (" + description + ") with extensions prevented");
    Object.preventExtensions(object);
    oldProto = Object.getPrototypeOf(object);
    shouldThrow("Object.setPrototypeOf(object, {})");
    shouldBe("Object.getPrototypeOf(object)", "oldProto");
}

// Ensure prototype change affects lookup.
debug(""); debug("Test prototype lookup");
object = {};
shouldBeFalse("'x' in object");
shouldBeFalse("'y' in object");

var oldProto = { x: 'old x', y: 'old y', };
shouldBe("Object.setPrototypeOf(object, oldProto)", "object");
shouldBe("object.x", "'old x'");
shouldBe("object.y", "'old y'");

var newProto = { x: 'new x' };
shouldBe("Object.setPrototypeOf(object, newProto)", "object");
shouldBe("object.x", "'new x'");
shouldBeFalse("'y' in object");

// Other prototype behavior, like instanceof and compare to __proto__.
debug(""); debug("Test other behavior");
shouldBeTrue("object = {}; Object.setPrototypeOf(object, Array.prototype); object instanceof Array");
shouldBeTrue("object = {}; Object.setPrototypeOf(object, Array.prototype); object.__proto__ === Array.prototype");
shouldBeTrue("object = {}; Object.setPrototypeOf(object, Array.prototype); Array.prototype.isPrototypeOf(object)");

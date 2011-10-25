description("Tests the properties of the RangeException object.")

var e;
try {
    document.createRange().setStartAfter(document, 0);
    // raises INVALID_NODE_TYPE_ERR
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "Error: INVALID_NODE_TYPE_ERR: DOM Range Exception 2");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object RangeException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object RangeExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "[object RangeExceptionConstructor]");
shouldBe("e.constructor", "window.RangeException");
shouldBe("e.INVALID_NODE_TYPE_ERR", "e.constructor.INVALID_NODE_TYPE_ERR");
shouldBe("e.BAD_BOUNDARYPOINTS_ERR", "1");
shouldBe("e.INVALID_NODE_TYPE_ERR", "2");

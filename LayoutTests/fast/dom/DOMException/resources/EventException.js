description("Tests the properties of the EventException object.")

var e;
try {
    document.dispatchEvent(null);
    // raises a UNSPECIFIED_EVENT_TYPE_ERR
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "Error: UNSPECIFIED_EVENT_TYPE_ERR: DOM Events Exception 0");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object EventException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object EventExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "[object EventExceptionConstructor]");
shouldBe("e.constructor", "window.EventException");
shouldBe("e.UNSPECIFIED_EVENT_TYPE_ERR", "e.constructor.UNSPECIFIED_EVENT_TYPE_ERR");
shouldBe("e.UNSPECIFIED_EVENT_TYPE_ERR", "0");

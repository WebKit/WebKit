description("DOMException needs a real prototype: http://bugs.webkit.org/show_bug.cgi?id=16637")

var e;
try {
  document.body.appendChild(document.documentElement);
   // raises a HIERARCHY_REQUEST_ERR
} catch (err) {
  e = err;
}

shouldBeEqualToString("e.toString()", "Error: HIERARCHY_REQUEST_ERR: DOM Exception 3");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object DOMException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object DOMExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "[object DOMExceptionConstructor]");
shouldBe("e.constructor", "window.DOMException");
shouldBe("e.HIERARCHY_REQUEST_ERR", "e.constructor.HIERARCHY_REQUEST_ERR");
shouldBe("e.HIERARCHY_REQUEST_ERR", "3");

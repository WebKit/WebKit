description("Promises - Test basic types / exceptions.");

// Promise
debug("");
debug("Promises");
debug("");

// Promises should be of type Promise.

var aPromise = new Promise(function(resolve, reject) { resolve(1); });

debug("aPromise = new Promise(...)")
shouldBeType("aPromise", "Promise");
shouldBe("String(aPromise)", "'[object Promise]'");

shouldBeDefined("aPromise.then");
shouldBeType("aPromise.then", "Function");
shouldBe("aPromise.then.length", "2");
shouldBeDefined("aPromise.catch");
shouldBeType("aPromise.catch", "Function");
shouldBe("aPromise.catch.length", "1");

debug("aPromise2 = Promise(...)")
shouldThrow(`Promise(function(resolve, reject) { resolve(1); })`);

// Promise constructor
debug("");
debug("Promise constructor");
debug("");

// Need at least one parameter.
shouldBe("Promise.length", "1");
shouldThrow("new Promise()");
shouldThrow("Promise()");

// Parameter must be a function.
shouldThrow("new Promise(1)", "'TypeError: Promise constructor takes a function argument'");
shouldThrow("new Promise('hello')", "'TypeError: Promise constructor takes a function argument'");
shouldThrow("new Promise([])", "'TypeError: Promise constructor takes a function argument'");
shouldThrow("new Promise({})", "'TypeError: Promise constructor takes a function argument'");
shouldThrow("new Promise(null)", "'TypeError: Promise constructor takes a function argument'");
shouldThrow("new Promise(undefined)", "'TypeError: Promise constructor takes a function argument'");

shouldThrow("Promise(1)", "'TypeError: calling Promise constructor without new is invalid'");
shouldThrow("Promise('hello')", "'TypeError: calling Promise constructor without new is invalid'");
shouldThrow("Promise([])", "'TypeError: calling Promise constructor without new is invalid'");
shouldThrow("Promise({})", "'TypeError: calling Promise constructor without new is invalid'");
shouldThrow("Promise(null)", "'TypeError: calling Promise constructor without new is invalid'");
shouldThrow("Promise(undefined)", "'TypeError: calling Promise constructor without new is invalid'");

// Promise statics
debug("");
debug("Promise statics");
debug("");


// Need at least one parameter.
shouldBeType("Promise.resolve", "Function");
shouldBe("Promise.resolve.length", "1");
shouldNotThrow("Promise.resolve(1)");

shouldBeType("Promise.reject", "Function");
shouldBe("Promise.reject.length", "1");
shouldNotThrow("Promise.reject(1)");

// Should return Promise objects.
shouldBeType("Promise.resolve(1)", "Promise");
shouldBeType("Promise.reject(1)", "Promise");



// Error constructor called as a function
shouldBe("Error('msg').message", "'msg'");

// Error Constructor called as part of a new expression
shouldBe("(new Error('msg')).message", "'msg'");
// moved to evil-n.js shouldBeUndefined("(new Error()).message");
shouldBe("(new Error('msg')).name", "'Error'");

shouldBe("Object.prototype.toString.apply(Error())", "'[object Error]'");
shouldBe("Object.prototype.toString.apply(Error)", "'[object Function]'");
shouldBe("Object.prototype.toString.apply(EvalError)", "'[object Function]'");
successfullyParsed = true

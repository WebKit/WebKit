shouldBe("Object.prototype.length","undefined");
shouldBe("Function.prototype.length","0");
shouldBe("Array.prototype.length","0");
shouldBe("String.prototype.length","0");
shouldBe("Boolean.prototype.length","undefined");
shouldBe("Number.prototype.length","undefined");
shouldBe("Date.prototype.length","undefined");
shouldBe("RegExp.prototype.length","undefined");
shouldBe("Error.prototype.length","undefined");

// check !ReadOnly
Array.prototype.length = 6;
shouldBe("Array.prototype.length","6");
// check ReadOnly
Function.prototype.length = 7;
shouldBe("Function.prototype.length","0");
String.prototype.length = 8;
shouldBe("String.prototype.length","0");

// check DontDelete
shouldBe("delete Array.prototype.length","false");
shouldBe("delete Function.prototype.length","false");
shouldBe("delete String.prototype.length","false");

// check DontEnum
var foundArrayPrototypeLength = false;
for (i in Array.prototype) { if (i == "length") foundArrayPrototypeLength = true; }
shouldBe("foundArrayPrototypeLength","false");

var foundFunctionPrototypeLength = false;
for (i in Function.prototype) { if (i == "length") foundFunctionPrototypeLength = true; }
shouldBe("foundFunctionPrototypeLength","false");

var foundStringPrototypeLength = false;
for (i in String.prototype) { if (i == "length") foundStringPrototypeLength = true; }
shouldBe("foundStringPrototypeLength","false");
successfullyParsed = true

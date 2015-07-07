var myfunc = Function();
shouldBe("myfunc.prototype.constructor.__proto__ == Function.prototype","true");
shouldBe("myfunc.prototype.constructor.__proto__.__proto__ == Object.prototype","true");
shouldBe("myfunc.prototype.constructor == myfunc","true");
shouldBe("Function.prototype.constructor == Function","true");
shouldBe("myfunc.prototype.toString == Object.prototype.toString","true");
shouldBe("myfunc.toString == Function.prototype.toString","true");

//printValue("myfunc"); = "(Internal function)" ### should display code
//printValue("myfunc.prototype.constructor"); = "(Internal function)" ### should display code

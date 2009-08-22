description("This test performs some simple check on the formnovalidate attribute.");

var i = document.createElement("input");
var b = document.createElement("button");

shouldBe("i.hasAttribute('formNoValidate')", "false");
shouldBe("i.getAttribute('formNoValidate')", "null");
shouldBe("i.formNoValidate", "false");
shouldBe("b.hasAttribute('formNoValidate')", "false");
shouldBe("b.getAttribute('formNoValidate')", "null");
shouldBe("b.formNoValidate", "false");

i.formNoValidate = true;
b.formNoValidate = true;

shouldBe("i.hasAttribute('formNoValidate')", "true");
shouldBe("i.getAttribute('formNoValidate')", "''");
shouldBe("i.formNoValidate", "true");
shouldBe("b.hasAttribute('formNoValidate')", "true");
shouldBe("b.getAttribute('formNoValidate')", "''");
shouldBe("b.formNoValidate", "true");

var i2 = document.createElement("input");
i2.formNoValidate = i.formNoValidate;

i.formNoValidate = false;
b.formNoValidate = false;

shouldBe("i.hasAttribute('formNoValidate')", "false");
shouldBe("i.getAttribute('formNoValidate')", "null");
shouldBe("i.formNoValidate", "false");
shouldBe("b.hasAttribute('formNoValidate')", "false");
shouldBe("b.getAttribute('formNoValidate')", "null");
shouldBe("b.formNoValidate", "false");

i2.formNoValidate = false;
shouldBe("i2.formNoValidate", "false");
i2.formNoValidate = "something";
shouldBe("i2.hasAttribute('formNoValidate')", "true");
shouldBe("i2.getAttribute('formNoValidate')", "''");
shouldBe("i2.formNoValidate", "true");

var successfullyParsed = true;

description("This test performs some simple check on the noValidate attribute.");

var f = document.createElement("form");

shouldBe("f.hasAttribute('noValidate')", "false");
shouldBe("f.getAttribute('noValidate')", "null");
shouldBe("f.noValidate", "false");

f.noValidate = true;

shouldBe("f.hasAttribute('noValidate')", "true");
shouldBe("f.getAttribute('noValidate')", "''");
shouldBe("f.noValidate", "true");

var f2 = document.createElement("form");
f2.noValidate = f.noValidate;

f.noValidate = false;

shouldBe("f.hasAttribute('noValidate')", "false");
shouldBe("f.getAttribute('noValidate')", "null");
shouldBe("f.noValidate", "false");

shouldBe("f2.hasAttribute('noValidate')", "true");
shouldBe("f2.getAttribute('noValidate')", "''");
shouldBe("f2.noValidate", "true");

f2.noValidate = false;

shouldBe("f2.noValidate", "false");
f2.noValidate = "something";
shouldBe("f2.hasAttribute('noValidate')", "true");
shouldBe("f2.getAttribute('noValidate')", "''");
shouldBe("f2.noValidate", "true");

var successfullyParsed = true;

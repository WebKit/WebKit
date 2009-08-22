description("This test performs some simple check on the novalidate attribute.");

var f = document.createElement("form");

shouldBe("f.hasAttribute('novalidate')", "false");
shouldBe("f.getAttribute('novalidate')", "null");
shouldBe("f.novalidate", "false");

f.novalidate = true;

shouldBe("f.hasAttribute('novalidate')", "true");
shouldBe("f.getAttribute('novalidate')", "''");
shouldBe("f.novalidate", "true");

var f2 = document.createElement("form");
f2.novalidate = f.novalidate;

f.novalidate = false;

shouldBe("f.hasAttribute('novalidate')", "false");
shouldBe("f.getAttribute('novalidate')", "null");
shouldBe("f.novalidate", "false");

shouldBe("f2.hasAttribute('novalidate')", "true");
shouldBe("f2.getAttribute('novalidate')", "''");
shouldBe("f2.novalidate", "true");

f2.novalidate = false;

shouldBe("f2.novalidate", "false");
f2.novalidate = "something";
shouldBe("f2.hasAttribute('novalidate')", "true");
shouldBe("f2.getAttribute('novalidate')", "''");
shouldBe("f2.novalidate", "true");

var successfullyParsed = true;

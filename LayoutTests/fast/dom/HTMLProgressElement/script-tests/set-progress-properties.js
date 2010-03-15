description('Test setting valid and invalid properties of HTMLProgressElement.');

var p = document.createElement('progress');

debug("Test values before properties were set");
shouldBe("p.value", "1");
shouldBe("p.max", "1");
shouldBe("p.position", "-1");

debug("Set valid values");
p.value = 7e1;
p.max = "1e2";
shouldBe("p.value", "70");
shouldBe("p.max", "100");
shouldBe("p.position", "0.7");

debug("Set value bigger than max");
p.value = 200;
p.max = 100.0;
shouldBe("p.value", "100");
shouldBe("p.max", "100");
shouldBe("p.position", "1");

debug("Set invalid value");
p.value = "200A";
p.max = 100;
shouldBe("p.value", "0");
shouldBe("p.max", "100");
shouldBe("p.position", "0");

debug("Set invalid max");
p.value = "20";
p.max = "max";
shouldBe("p.value", "1");
shouldBe("p.max", "1");
shouldBe("p.position", "1");

debug("Set value to null and max to 0");
p.value = null;
p.max = 0;
shouldBe("p.value", "0");
shouldBe("p.max", "1");
shouldBe("p.position", "0");

debug("Set attributes to valid numbers");
p.setAttribute("value", 5);
p.setAttribute("max", 10);
shouldBe("p.value", "5");
shouldBe("p.max", "10");
shouldBe("parseInt(p.getAttribute('value'))", "5");
shouldBe("parseInt(p.getAttribute('max'))", "10");

debug("Set attributes to invalid values");
p.setAttribute("value", "ABC");
p.setAttribute("max", "#");
shouldBe("p.value", "0");
shouldBe("p.max", "1");
shouldBe("p.getAttribute('value')", "'ABC'");
shouldBe("p.getAttribute('max')", "'#'");

var successfullyParsed = true;

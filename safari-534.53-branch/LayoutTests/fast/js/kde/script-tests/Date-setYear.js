debug("Start Of Test");

var d = new Date();
shouldBe("d.setYear(-1), d.getFullYear()", "-1");
shouldBe("d.setYear(0), d.getFullYear()", "1900");
shouldBe("d.setYear(1), d.getFullYear()", "1901");
shouldBe("d.setYear(99), d.getFullYear()", "1999");
shouldBe("d.setYear(100), d.getFullYear()", "100");
shouldBe("d.setYear(2050), d.getFullYear()", "2050");
shouldBe("d.setYear(1899), d.getFullYear()", "1899");
shouldBe("d.setYear(2000), d.getFullYear()", "2000");
shouldBe("d.setYear(2100), d.getFullYear()", "2100");

successfullyParsed = true

description("This test checks the behavior of the remove() method on the select.options object.");

var select1 = document.getElementById("select1");
var value;

debug("1.1 Remove (object) from empty Options");
value = document.createElement("DIV");
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.2 Remove (string) from empty Options");
value = "o";
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.3 Remove (float) from empty Options");
value = 3.14;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.4 Remove (boolean) from empty Options");
value = true;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.5 Remove (undefined) from empty Options");
value = undefined;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.6 Remove (null) from empty Options");
value = null;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.7 Remove (negative infinity) from empty Options");
value = -1/0;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.8 Remove (NaN) from empty Options");
value = 0/0;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.9 Remove (positive infinity) from empty Options");
value = 1/0;
shouldBe("select1.options.remove(value)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.10 Remove no args from empty Options");
shouldBe("select1.options.remove()", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.11 Remove too many args from empty Options");
shouldBe("select1.options.remove(0, 'foo')", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.12 Remove invalid index -2 from empty Options");
shouldBe("select1.options.remove(-2)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.13 Remove invalid index -1 from empty Options");
shouldBe("select1.options.remove(-1)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.14 Remove index 0 from empty Options");
shouldBe("select1.options.remove(0)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

debug("1.15 Remove index 1 from empty Options");
shouldBe("select1.options.remove(1)", "undefined");
shouldBe("select1.options.length", "0");
shouldBe("select1.selectedIndex", "-1");
debug("");

// ------------------------------------------------

i = 0;
var select2 = document.getElementById("select2");

debug("2.1 Remove (object) from non-empty Options");
value = document.createElement("DIV");
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "15");
shouldBe("select2.selectedIndex", "13");
shouldBe("select2.options[0].value", "'B'");
debug("");

debug("2.2 Remove (string) from non-empty Options");
value = "o";
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "14");
shouldBe("select2.selectedIndex", "12");
shouldBe("select2.options[0].value", "'C'");
debug("");

debug("2.3 Remove (float) from non-empty Options");
value = 3.14;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "13");
shouldBe("select2.selectedIndex", "11");
shouldBe("select2.options[3].value", "'G'");
debug("");

debug("2.4 Remove (boolean true) from non-empty Options");
value = true;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "12");
shouldBe("select2.selectedIndex", "10");
shouldBe("select2.options[1].value", "'E'");
debug("");

debug("2.5 Remove (boolean false) from non-empty Options");
value = false;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "11");
shouldBe("select2.selectedIndex", "9");
shouldBe("select2.options[1].value", "'G'");
debug("");

debug("2.6 Remove (undefined) from non-empty Options");
value = undefined;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "10");
shouldBe("select2.selectedIndex", "8");
shouldBe("select2.options[0].value", "'G'");
debug("");

debug("2.7 Remove (null) from non-empty Options");
value = null;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "9");
shouldBe("select2.selectedIndex", "7");
shouldBe("select2.options[0].value", "'H'");
debug("");

debug("2.8 Remove (negative infinity) from non-empty Options");
value = -1/0;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "8");
shouldBe("select2.selectedIndex", "6");
shouldBe("select2.options[0].value", "'I'");
debug("");

debug("2.9 Remove (NaN) from non-empty Options");
value = 0/0;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "5");
shouldBe("select2.options[0].value", "'J'");
debug("");

debug("2.10 Remove (positive infinity) from non-empty Options");
value = 1/0;
shouldBe("select2.options.remove(value)", "undefined");
shouldBe("select2.options.length", "6");
shouldBe("select2.selectedIndex", "4");
shouldBe("select2.options[0].value", "'K'");
debug("");

debug("2.11 Remove no args from non-empty Options");
shouldBe("select2.options.remove()", "undefined");
shouldBe("select2.options.length", "5");
shouldBe("select2.selectedIndex", "3");
shouldBe("select2.options[0].value", "'L'");
debug("");

debug("2.12 Remove too many args from non-empty Options");
shouldBe("select2.options.remove(0, 'foo')", "undefined");
shouldBe("select2.options.length", "4");
shouldBe("select2.selectedIndex", "2");
shouldBe("select2.options[0].value", "'M'");
debug("");

debug("2.13 Remove invalid index -2 from non-empty Options");
shouldBe("select2.options.remove(-2)", "undefined");
shouldBe("select2.options.length", "4");
shouldBe("select2.selectedIndex", "2");
shouldBe("select2.options[2].value", "'O'");
debug("");

debug("2.14 Remove invalid index -1 from non-empty Options");
shouldBe("select2.options.remove(-1)", "undefined");
shouldBe("select2.options.length", "4");
shouldBe("select2.selectedIndex", "2");
shouldBe("select2.options[3].value", "'P'");
debug("");

debug("2.15 Remove index 0 from non-empty Options");
shouldBe("select2.options.remove(0)", "undefined");
shouldBe("select2.options.length", "3");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'N'");
debug("");

debug("2.16 Remove index 1 from non-empty Options");
shouldBe("select2.options.remove(1)", "undefined");
shouldBe("select2.options.length", "2");
shouldBe("select2.selectedIndex", "0");
shouldBe("select2.options[1].value", "'P'");
debug("");

successfullyParsed = true;

description(
"This test checks the behavior of the add() method on the select.options object.<br>" +
"It covers both the the one-argument (1.x) and two-argument (2.x) signatures of the add() method."
);

div = document.createElement("div");
sel = document.createElement("select");
sel.setAttribute("id", "select1");
div.appendChild(sel);
sel = document.createElement("select");
sel.setAttribute("id", "select2");
div.appendChild(sel);
document.body.insertBefore(div, document.getElementById("console").nextSibling);

debug("1.1 Add Option to empty Options");
var select1 = document.getElementById("select1");
var option1 = document.createElement("OPTION");
select1.options.add(option1);
option1.value = "1";
option1.textContent = "A";
shouldBe("select1.options.length", "1");
shouldBe("select1.selectedIndex", "0");
shouldBe("select1.options[0].value", "'1'");
shouldBe("select1.options[0].textContent", "'A'");
debug("");

debug("1.2 Add Option to non-empty Options");
option1 = document.createElement("OPTION");
select1.options.add(option1);
option1.value = "2";
option1.textContent = "B";
shouldBe("select1.options.length", "2");
shouldBe("select1.selectedIndex", "0");
shouldBe("select1.options[0].value", "'1'");
shouldBe("select1.options[0].textContent", "'A'");
shouldBe("select1.options[1].value", "'2'");
shouldBe("select1.options[1].textContent", "'B'");
debug("");

debug("1.3 Add Option after setting parameters");
option1 = document.createElement("OPTION");
option1.value = "3";
option1.textContent = "C";
select1.options.add(option1);
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
shouldBe("select1.options[0].value", "'1'");
shouldBe("select1.options[0].textContent", "'A'");
shouldBe("select1.options[1].value", "'2'");
shouldBe("select1.options[1].textContent", "'B'");
shouldBe("select1.options[2].value", "'3'");
shouldBe("select1.options[2].textContent", "'C'");
debug("");

debug("1.4 Add a non-Option element");
option1 = document.createElement("DIV");
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.5 Add a non-element (string)");
option1 = "o";
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.6 Add a non-element (number)");
option1 = 3.14;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.7 Add a non-element (boolean)");
option1 = true;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.8 Add undefined");
option1 = undefined;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.9 Add null");
option1 = null;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.10 Add negative infinity");
option1 = -1/0;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.11 Add NaN");
option1 = 0/0;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("1.12 Add positive infinity");
option1 = 1/0;
shouldThrow("select1.options.add(option1)");
shouldBe("select1.options.length", "3");
shouldBe("select1.selectedIndex", "0");
debug("");

debug("2.1 Add Option to empty Options");
var select2 = document.getElementById("select2");
var option2 = document.createElement("OPTION");
select2.options.add(option2, 0);
option2.value = "1";
option2.textContent = "A";
shouldBe("select2.options.length", "1");
shouldBe("select2.selectedIndex", "0");
shouldBe("select2.options[0].value", "'1'");
shouldBe("select2.options[0].textContent", "'A'");
debug("");

debug("2.2 Add Option after setting parameters");
option2 = document.createElement("OPTION");
option2.value = "2";
option2.textContent = "B";
select2.options.add(option2, 1);
shouldBe("select2.options.length", "2");
shouldBe("select2.selectedIndex", "0");
shouldBe("select2.options[0].value", "'1'");
shouldBe("select2.options[0].textContent", "'A'");
shouldBe("select2.options[1].value", "'2'");
shouldBe("select2.options[1].textContent", "'B'");
debug("");

debug("2.3 Insert Option at beginning of Options");
option2 = document.createElement("OPTION");
select2.options.add(option2, 0);
option2.value = "0";
option2.textContent = "Z";
shouldBe("select2.options.length", "3");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'0'");
shouldBe("select2.options[0].textContent", "'Z'");
shouldBe("select2.options[1].value", "'1'");
shouldBe("select2.options[1].textContent", "'A'");
shouldBe("select2.options[2].value", "'2'");
shouldBe("select2.options[2].textContent", "'B'");
debug("");

debug("2.4 Insert Option in middle of Options");
option2 = document.createElement("OPTION");
select2.options.add(option2, 2);
option2.value = "1.5";
option2.textContent = "A.5";
shouldBe("select2.options.length", "4");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'0'");
shouldBe("select2.options[0].textContent", "'Z'");
shouldBe("select2.options[1].value", "'1'");
shouldBe("select2.options[1].textContent", "'A'");
shouldBe("select2.options[2].value", "'1.5'");
shouldBe("select2.options[2].textContent", "'A.5'");
shouldBe("select2.options[3].value", "'2'");
shouldBe("select2.options[3].textContent", "'B'");
debug("");

debug("2.5 Insert Option at end of Options");
option2 = document.createElement("OPTION");
select2.options.add(option2, 4);
option2.value = "3";
option2.textContent = "C";
shouldBe("select2.options.length", "5");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'0'");
shouldBe("select2.options[0].textContent", "'Z'");
shouldBe("select2.options[1].value", "'1'");
shouldBe("select2.options[1].textContent", "'A'");
shouldBe("select2.options[2].value", "'1.5'");
shouldBe("select2.options[2].textContent", "'A.5'");
shouldBe("select2.options[3].value", "'2'");
shouldBe("select2.options[3].textContent", "'B'");
shouldBe("select2.options[4].value", "'3'");
shouldBe("select2.options[4].textContent", "'C'");
debug("");

debug("2.6 Insert Option beyond the end of Options");
option2 = document.createElement("OPTION");
select2.options.add(option2, 6);
option2.value = "4";
option2.textContent = "D";
shouldBe("select2.options.length", "6");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'0'");
shouldBe("select2.options[0].textContent", "'Z'");
shouldBe("select2.options[1].value", "'1'");
shouldBe("select2.options[1].textContent", "'A'");
shouldBe("select2.options[2].value", "'1.5'");
shouldBe("select2.options[2].textContent", "'A.5'");
shouldBe("select2.options[3].value", "'2'");
shouldBe("select2.options[3].textContent", "'B'");
shouldBe("select2.options[4].value", "'3'");
shouldBe("select2.options[4].textContent", "'C'");
shouldBe("select2.options[5].value", "'4'");
shouldBe("select2.options[5].textContent", "'D'");
debug("");

debug("2.7 Add an Option at index -1");
option2 = document.createElement("OPTION");
select2.options.add(option2, -1);
option2.value = "5";
option2.textContent = "E";
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
shouldBe("select2.options[0].value", "'0'");
shouldBe("select2.options[0].textContent", "'Z'");
shouldBe("select2.options[1].value", "'1'");
shouldBe("select2.options[1].textContent", "'A'");
shouldBe("select2.options[2].value", "'1.5'");
shouldBe("select2.options[2].textContent", "'A.5'");
shouldBe("select2.options[3].value", "'2'");
shouldBe("select2.options[3].textContent", "'B'");
shouldBe("select2.options[4].value", "'3'");
shouldBe("select2.options[4].textContent", "'C'");
shouldBe("select2.options[5].value", "'4'");
shouldBe("select2.options[5].textContent", "'D'");
shouldBe("select2.options[6].value", "'5'");
shouldBe("select2.options[6].textContent", "'E'");
debug("");

debug("2.8 Add an Option at index -2");
option2 = document.createElement("OPTION");
shouldThrow("select2.options.add(option2, -2)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.9 Add an Option at index -Infinity");
option2 = document.createElement("OPTION");
shouldThrow("select2.options.add(option2, -1/0)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.10 Add an Option at index NaN");
option2 = document.createElement("OPTION");
shouldThrow("select2.options.add(option2, 0/0)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.11 Add an Option at index Infinity");
option2 = document.createElement("OPTION");
shouldThrow("select2.options.add(option2, 1/0)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.12 Add a non-Option element");
option2 = document.createElement("DIV");
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.13 Add a non-element (string)");
option2 = "o";
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.14 Add a non-element (number)");
option2 = 3.14;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.15 Add a non-element (boolean)");
option2 = true;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.16 Add undefined");
option2 = undefined;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.17 Add null");
option2 = null;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.18 Add negative infinity");
option2 = -1/0;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.19 Add NaN");
option2 = 0/0;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

debug("2.20 Add positive infinity");
option2 = 1/0;
shouldThrow("select2.options.add(option2, 1)");
shouldBe("select2.options.length", "7");
shouldBe("select2.selectedIndex", "1");
debug("");

successfullyParsed = true;

description("Test for validationMessage DOM property.");

var form = document.createElement("form");

// An input element with a pattern set and a mismatched value
var patternInput = document.createElement("input");
patternInput.name = "patternInput";
patternInput.pattern = "lorem ipsum";
patternInput.value = "lorem";
form.appendChild(patternInput);
shouldBe("patternInput.validationMessage", "'pattern mismatch'");

// A required input with an empty value
var requiredInput = document.createElement("input");
requiredInput.name = "requiredInput";
requiredInput.required = true;
form.appendChild(requiredInput);
shouldBe("requiredInput.validationMessage", "'value missing'");

// A required textarea with an empty value
var requiredTextArea = document.createElement("textarea");
requiredTextArea.name = "requiredTextArea";
requiredTextArea.required = true;
form.appendChild(requiredTextArea);
shouldBe("requiredTextArea.validationMessage", "'value missing'");

// A type=email input for the "type mismatch" flag
var emailInput = document.createElement("input");
emailInput.name = "emailInput";
emailInput.type = "email";
emailInput.value = "incorrectValue";
form.appendChild(emailInput);
shouldBe("emailInput.validationMessage", "'type mismatch'");

// A "ranged" input for "range underflow" flag
var underInput = document.createElement("input");
underInput.name = "underInput";
underInput.type = "range";
underInput.min = "2";
underInput.value = "1";
form.appendChild(underInput);
shouldBe("underInput.validationMessage", "'range underflow'");

// A "ranged" input for "range overflow" flag
var overInput = document.createElement("input");
overInput.name = "overInput";
overInput.type = "range";
overInput.max = "2";
overInput.value = "3";
form.appendChild(overInput);
shouldBe("overInput.validationMessage", "'range overflow'");

// A button can't be valited and, thus, has a blank validationMessage
var but = document.createElement("button");
but.name = "button";
form.appendChild(but);
shouldBe("but.validationMessage", "''");

// An input control with no name, so it can't be validated (willValidate = false)
var anoninput = document.createElement("input");
form.appendChild(anoninput);
shouldBe("anoninput.validationMessage", "''")

// Fieldsets can't be validated
var happyFieldset = document.createElement("fieldset");
happyFieldset.name = "fieldset";
form.appendChild(happyFieldset);
shouldBe("happyFieldset.validationMessage", "''");

// Select controls can't be validated too
var happySelect = document.createElement("select");
happySelect.name = "select";
form.appendChild(happySelect);
shouldBe("happySelect.validationMessage", "''");

var successfullyParsed = true;

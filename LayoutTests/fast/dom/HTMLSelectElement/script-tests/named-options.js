description("This tests that options elements are accessible by name from both a select element and the options collection, per HTML5.");

var select1 = document.createElement("select");
document.body.appendChild(select1);
select1.innerHTML = '<option value="Value" name="test">';

var select2 = document.createElement("select");
document.body.appendChild(select2);
select2.innerHTML = '<option value="Value1" name="test"><option value="Value2" name="test">';

debug("Confirm that the option named 'test' is accessible from the select element");
shouldBeEqualToString("select1.test.toString()", "[object HTMLOptionElement]");
shouldBeEqualToString("select1.test.value", "Value");

debug("Confirm that the option named 'test' is accessible from the options collection");
shouldBeEqualToString("select1.options.test.toString()", "[object HTMLOptionElement]");
shouldBeEqualToString("select1.options.test.value", "Value");

debug("Confirm that both options named 'test' are accessible from the select element");
shouldBe("select2.test.length", "2");
shouldBeEqualToString("select2.test.toString()", "[object Collection]");
shouldBeEqualToString("select2.test[0].value", "Value1");
shouldBeEqualToString("select2.test[1].value", "Value2");

debug("Confirm that both options named 'test' are accessible from the options collection");
shouldBe("select2.options.test.length", "2");
shouldBeEqualToString("select2.options.test.toString()", "[object Collection]");
shouldBeEqualToString("select2.options.test[0].value", "Value1");
shouldBeEqualToString("select2.options.test[1].value", "Value2");

// Clean up after ourselves
document.body.removeChild(select1);
document.body.removeChild(select2);

successfullyParsed = true;

description("This tests that option elements are accessible using namedItem from both HTMLSelectElement and HTMLOptionsCollection as well as using the getter from the HTMLOptionsCollection");

var select1 = document.createElement("select");
document.body.appendChild(select1);
select1.innerHTML = '<option value="Value" name="test">';

var select2 = document.createElement("select");
document.body.appendChild(select2);
select2.innerHTML = '<option value="Value1" name="test"><option value="Value2" name="test">';

debug("Confirm that the option named 'test' is accessible from the select element");
shouldBeEqualToString("select1.namedItem('test').toString()", "[object HTMLOptionElement]");
shouldBeEqualToString("select1.namedItem('test').value", "Value");

debug("Confirm that the option named 'test' is accessible from the options collection");
shouldBeEqualToString("select1.options.namedItem('test').toString()", "[object HTMLOptionElement]");
shouldBeEqualToString("select1.options.namedItem('test').value", "Value");

debug("Confirm that both options named 'test' are accessible from the options collection");
shouldBe("select2.options.namedItem('test').length", "2");
shouldBeEqualToString("select2.options.namedItem('test').toString()", "[object NodeList]");
shouldBeEqualToString("select2.options.namedItem('test')[0].value", "Value1");
shouldBeEqualToString("select2.options.namedItem('test')[1].value", "Value2");

shouldBe("select2.options.test.length", "2");
shouldBeEqualToString("select2.options.test.toString()", "[object NodeList]");
shouldBeEqualToString("select2.options.test[0].value", "Value1");
shouldBeEqualToString("select2.options.test[1].value", "Value2");

// Clean up after ourselves
document.body.removeChild(select1);
document.body.removeChild(select2);

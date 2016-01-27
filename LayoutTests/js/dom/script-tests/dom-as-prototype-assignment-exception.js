description(
'Tests to make sure we throw when triggering a custom property with a mismatched this'
);


var div = document.createElement("div")
div.id = "test"
var testObject = { __proto__: div}
// Needed for compatibility with weird websites.
shouldThrow('testObject.id')
shouldThrow('testObject.id="foo"')

testObject = {__proto__: document.getElementsByTagName("div")}
shouldBe("testObject.length", '1')

shouldBe("div.id", "'test'")
shouldBeFalse("div.hasOwnProperty('id')")
shouldThrow("div.__proto__.id")


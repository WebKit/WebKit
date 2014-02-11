description(
'Tests to make sure we throw when triggering a custom property with a mismatched this'
);


var div = document.createElement("div")
div.id = "test"
var testObject = { __proto__: div}
// Needed for compatability with weird websites
shouldBeUndefined('testObject.id')
shouldThrow('testObject.id="foo"')

testObject = {__proto__: document.getElementsByTagName("div")}
shouldThrow("testObject.length")

shouldBe("div.id", "'test'")
shouldBeFalse("div.hasOwnProperty('id')")
shouldBeUndefined("div.__proto__.id")


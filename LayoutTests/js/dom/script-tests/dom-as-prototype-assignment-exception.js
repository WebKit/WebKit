description(
'Tests to make sure we throw when triggering a custom property with a mismatched this'
);



var div = document.createElement("div")
var testObject = { __proto__: div}
shouldThrow('testObject.id')
shouldThrow('testObject.id="foo"')

testObject = {__proto__: document.getElementsByTagName("div")}
shouldThrow("testObject.length")


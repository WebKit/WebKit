description(
"This tests that NSResolver throw the correct exceptions."
);

shouldThrow("document.querySelector(\"xh|p\", function(){});", "'Error: NAMESPACE_ERR: DOM Exception 14'");
shouldThrow("document.querySelector(\"xh|p\", function(){ throw \"error\"; });", "'error'");
shouldThrow("document.querySelector(\"xh|p\", {});", "'Error: NAMESPACE_ERR: DOM Exception 14'");
shouldThrow("document.querySelector(\"xh|p\", { lookupNamespaceURI: function(){} });", "'Error: NAMESPACE_ERR: DOM Exception 14'");
shouldThrow("document.querySelector(\"xh|p\", { lookupNamespaceURI: function(){ throw \"error\"; } });", "'error'");
shouldThrow("document.querySelector(\"xh|p\", { get lookupNamespaceURI() { throw \"error\"; } });", "'error'");
shouldThrow("document.querySelector(\"xh|p\", null);", "'Error: NAMESPACE_ERR: DOM Exception 14'");
shouldThrow("document.querySelector(\"xh|p\", undefined);", "'Error: NAMESPACE_ERR: DOM Exception 14'");
shouldThrow("document.querySelector(\"xh|p\", \"A String\");", "'Error: NAMESPACE_ERR: DOM Exception 14'");


var successfullyParsed = true;

description("This test verifies that a StyleSheet object will be returned instead of a HTMLStyleElement when calling document.styleSheets named property getter.");

// The type of returned object by number getter should be equal with the named property getter.
shouldBe('document.styleSheets["test"].toString()', 'document.styleSheets[1].toString()');
    
var successfullyParsed = true;

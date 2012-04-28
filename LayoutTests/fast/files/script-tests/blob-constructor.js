description("Test the Blob constructor.");

// Test the diffrent ways you can construct a Blob.
shouldBeTrue("(new Blob()) instanceof window.Blob");
shouldBeTrue("(new Blob([])) instanceof window.Blob");
shouldBeTrue("(new Blob(['hello'])) instanceof window.Blob");
shouldBeTrue("(new Blob(['hello'], {})) instanceof window.Blob");
shouldBeTrue("(new Blob(['hello'], {type:'text/html'})) instanceof window.Blob");
shouldBeTrue("(new Blob(['hello'], {type:'text/html', endings:'native'})) instanceof window.Blob");
shouldBeTrue("(new Blob(['hello'], {type:'text/html', endings:'transparent'})) instanceof window.Blob");

// Test some error cases
shouldThrow("new Blob('hello')");
shouldThrow("new Blob(0)");

// Test valid blob parts.
shouldBeTrue("(new Blob([])) instanceof window.Blob");
shouldBeTrue("(new Blob([new ArrayBuffer(8)])) instanceof window.Blob");
shouldBeTrue("(new Blob(['stringPrimitive'])) instanceof window.Blob");
shouldBeTrue("(new Blob([String('stringObject')])) instanceof window.Blob");
shouldBeTrue("(new Blob([new Blob])) instanceof window.Blob");
shouldBeTrue("(new Blob([new Blob([new Blob])])) instanceof window.Blob");

// Test some conversions to string in the parts array.
shouldBe("(new Blob([12])).size", "2");
shouldBe("(new Blob([[]])).size", "0");         // [].toString() is the empty string
shouldBe("(new Blob([{}])).size", "15");;       // {}.toString() is the string "[object Object]"
shouldBe("(new Blob([document])).size", "21");  // document.toString() is the string "[object HTMLDocument]" 

var toStringingObj = { toString: function() { return "A string"; } };
shouldBe("(new Blob([toStringingObj])).size", "8");

var throwingObj = { toString: function() { throw "error"; } };
shouldThrow("new Blob([throwingObj])");


// Test some invalid property bags
shouldBeTrue("(new Blob([], {unknownKey:'value'})) instanceof window.Blob");    // Ignore invalid keys
shouldThrow("new Blob([], {endings:'illegalValue'})");                          // Throw for invalid value to endings
shouldThrow("new Blob([], {endings:throwingObj})");                             // Throws during toString
shouldThrow("new Blob([], {type:throwingObj})");                                // Throws during toString

shouldBeTrue("(new Blob([], null)) instanceof window.Blob");
shouldBeTrue("(new Blob([], undefined)) instanceof window.Blob");
shouldBeTrue("(new Blob([], 123)) instanceof window.Blob");
shouldBeTrue("(new Blob([], 123.4)) instanceof window.Blob");
shouldBeTrue("(new Blob([], [])) instanceof window.Blob");
shouldBeTrue("(new Blob([], true)) instanceof window.Blob");
shouldBeTrue("(new Blob([], 'abc')) instanceof window.Blob");
shouldBeTrue("(new Blob([], /abc/)) instanceof window.Blob");
shouldBeTrue("(new Blob([], function () {})) instanceof window.Blob");


// Test that the type/size is correctly added to the Blob
shouldBe("(new Blob([], {type:'text/html'})).type", "'text/html'");
shouldBe("(new Blob([], {type:'text/html'})).size", "0");

// Odds and ends
shouldBe("window.Blob.length", "2");

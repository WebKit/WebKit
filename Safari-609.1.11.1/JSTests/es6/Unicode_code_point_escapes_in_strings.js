function test() {

return '\u{1d306}' == '\ud834\udf06';
      
}

if (!test())
    throw new Error("Test failed");


function test() {

var s = Object.keys('a');
return s.length === 1 && s[0] === '0';
      
}

if (!test())
    throw new Error("Test failed");


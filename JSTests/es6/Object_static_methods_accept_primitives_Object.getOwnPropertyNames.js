function test() {

var s = Object.getOwnPropertyNames('a');
return s.length === 2 &&
  ((s[0] === 'length' && s[1] === '0') || (s[0] === '0' && s[1] === 'length'));
      
}

if (!test())
    throw new Error("Test failed");


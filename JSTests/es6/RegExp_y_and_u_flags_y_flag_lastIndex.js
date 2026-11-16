function test() {

var re = new RegExp('yy', 'y');
re.lastIndex = 3;
var result = re.exec('xxxyyxx')[0];
return result === 'yy' && re.lastIndex === 5;
      
}

if (!test())
    throw new Error("Test failed");


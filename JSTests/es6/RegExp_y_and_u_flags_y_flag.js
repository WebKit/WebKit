function test() {

var re = new RegExp('\\w', 'y');
re.exec('xy');
return (re.exec('xy')[0] === 'y');
      
}

if (!test())
    throw new Error("Test failed");


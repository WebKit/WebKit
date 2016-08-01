function test() {

var {toFixed} = 2;
var {slice} = '';
var toString, match;
({toString} = 2);
({match} = '');
return toFixed === Number.prototype.toFixed
  && toString === Number.prototype.toString
  && slice === String.prototype.slice
  && match === String.prototype.match;
      
}

if (!test())
    throw new Error("Test failed");


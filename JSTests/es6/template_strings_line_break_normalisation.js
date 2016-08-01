function test() {

var cr   = eval("`x" + String.fromCharCode(13)    + "y`");
var lf   = eval("`x" + String.fromCharCode(10)    + "y`");
var crlf = eval("`x" + String.fromCharCode(13,10) + "y`");

return cr.length === 3 && lf.length === 3 && crlf.length === 3
  && cr[1] === lf[1] && lf[1] === crlf[1] && crlf[1] === '\n';
      
}

if (!test())
    throw new Error("Test failed");


function test() {

var i, names = ["anchor", "big", "bold", "fixed", "fontcolor", "fontsize",
  "italics", "link", "small", "strike", "sub", "sup"];
for (i = 0; i < names.length; i++) {
  if (typeof String.prototype[names[i]] !== 'function') {
    return false;
  }
}
return true;
      
}

if (!test())
    throw new Error("Test failed");


function test() {

var i, names = ["anchor", "fontcolor", "fontsize", "link"];
for (i = 0; i < names.length; i++) {
  if (""[names[i]]('"') !== ""[names[i]]('&' + 'quot;')) {
    return false;
  }
}
return true;
      
}

if (!test())
    throw new Error("Test failed");


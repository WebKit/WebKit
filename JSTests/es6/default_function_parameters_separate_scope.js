function test() {

return (function(a=function(){
  return typeof b === 'undefined';
}){
  var b = 1;
  return a();
}());
      
}

if (!test())
    throw new Error("Test failed");


function test() {

function correctProtoBound(proto) {
  var f = ()=>5;
  if (Object.setPrototypeOf) {
    Object.setPrototypeOf(f, proto);
  }
  else {
    f.__proto__ = proto;
  } 
  var boundF = Function.prototype.bind.call(f, null);
  return Object.getPrototypeOf(boundF) === proto;
}
return correctProtoBound(Function.prototype)
  && correctProtoBound({})
  && correctProtoBound(null);
      
}

if (!test())
    throw new Error("Test failed");


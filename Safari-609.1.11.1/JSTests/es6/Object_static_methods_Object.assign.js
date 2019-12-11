function test() {

var o = Object.assign({a:true}, {b:true}, {c:true});
return "a" in o && "b" in o && "c" in o;
      
}

if (!test())
    throw new Error("Test failed");


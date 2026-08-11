function test() {

var sent;
function * generator(){
  sent = [yield 5, yield 6];
};
var iterator = generator();
iterator.next();
iterator.next("foo");
iterator.next("bar");
return sent[0] === "foo" && sent[1] === "bar";
      
}

if (!test())
    throw new Error("Test failed");


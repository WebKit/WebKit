function test() {

function C() {
  return x => new.target;
}
return new C()() === C && C()() === undefined;
      
}

if (!test())
    throw new Error("Test failed");


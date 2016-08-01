function test() {

function foo() {};
return foo.bind({}).name === "bound foo" &&
  (function(){}).bind({}).name === "bound ";
      
}

if (!test())
    throw new Error("Test failed");


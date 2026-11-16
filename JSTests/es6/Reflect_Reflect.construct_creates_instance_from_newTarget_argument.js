function test() {

function F(){}
return Reflect.construct(function(){}, [], F) instanceof F;
      
}

if (!test())
    throw new Error("Test failed");


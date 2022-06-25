function test() {

[] = [1,2];
({} = {a:1,b:2});
return true;
      
}

if (!test())
    throw new Error("Test failed");


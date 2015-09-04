function test() {

var iterable = global.__createIterableObject(["b", "c", "d"]);
return ["a", ...iterable, "e"][3] === "d";
      
}

if (!test())
    throw new Error("Test failed");


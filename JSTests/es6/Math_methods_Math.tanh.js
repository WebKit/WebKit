function test() {

return typeof Math.tanh === "function";

}

if (!test())
    throw new Error("Test failed");


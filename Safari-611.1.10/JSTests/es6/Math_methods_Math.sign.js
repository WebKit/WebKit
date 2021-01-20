function test() {

return typeof Math.sign === "function";

}

if (!test())
    throw new Error("Test failed");


function test() {

return typeof Math.log2 === "function";

}

if (!test())
    throw new Error("Test failed");


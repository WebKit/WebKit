function test() {

return typeof Math.asinh === "function";

}

if (!test())
    throw new Error("Test failed");


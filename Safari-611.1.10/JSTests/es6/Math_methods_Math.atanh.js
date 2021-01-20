function test() {

return typeof Math.atanh === "function";

}

if (!test())
    throw new Error("Test failed");


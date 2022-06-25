function test() {

return typeof Math.trunc === "function";

}

if (!test())
    throw new Error("Test failed");


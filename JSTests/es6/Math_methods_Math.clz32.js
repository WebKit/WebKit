function test() {

return typeof Math.clz32 === "function";

}

if (!test())
    throw new Error("Test failed");


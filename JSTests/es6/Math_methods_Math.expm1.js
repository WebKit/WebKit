function test() {

return typeof Math.expm1 === "function";

}

if (!test())
    throw new Error("Test failed");


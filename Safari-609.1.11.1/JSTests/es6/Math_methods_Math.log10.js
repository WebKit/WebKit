function test() {

return typeof Math.log10 === "function";

}

if (!test())
    throw new Error("Test failed");


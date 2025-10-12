function shouldBe(a, b) {
    if (a !== b)
        throw new Error("bad value");
}

{
    const jsTag1 = WebAssembly.JSTag;
    const jsTag2 = WebAssembly.JSTag;
    shouldBe(jsTag1, jsTag2);
}

{
    const jsTagDesc = Object.getOwnPropertyDescriptor(WebAssembly, "JSTag");
    const jsTagGetter = jsTagDesc.get;
    const jsTag1 = jsTagGetter.call({});
    const jsTag2 = jsTagGetter.call({});
    shouldBe(jsTag1, jsTag2);
}

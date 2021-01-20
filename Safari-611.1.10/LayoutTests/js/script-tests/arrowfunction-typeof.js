var af1 = () => {};
var af2 = (a) => {a + 1};

shouldBe("typeof af1 === 'function'", "true");

shouldBe("typeof af2 === 'function'", "true");

shouldBe("typeof (()=>{}) === 'function'", "true");

shouldBe("typeof ((b) => {b + 1})==='function'", "true");

var successfullyParsed = true;

description("typeof document.all is never 'function'");
const documentAll = document.all;

function testTypeofIsFunction() {
    let acc = 0;
    for (let i = 0; i < 1e6; ++i)
        acc += (typeof documentAll === "function");
    return acc === 1e6;
}

function testTypeofIsNotFunction() {
    let acc = 0;
    for (let i = 0; i < 1e6; ++i)
        acc += (typeof documentAll !== "function");
    return acc === 1e6;
}

shouldBeFalse("testTypeofIsFunction()");
shouldBeTrue("testTypeofIsNotFunction()");

description("typeof document.all is never 'object'");

const documentAll = document.all;
function testTypeofIsObject() {
    let acc = 0;
    for (let i = 0; i < 1e6; ++i)
        acc += (typeof documentAll === "object");

    if (acc !== 0)
        return false;

    for (let i = 0; i < 1e6; ++i)
        acc += (typeof documentAll !== "object");
    return acc === 1e6;
}

shouldBe("testTypeofIsObject()", "true");

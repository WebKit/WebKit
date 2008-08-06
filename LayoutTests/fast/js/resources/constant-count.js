description(
"This test checks exceptional cases for constant counting in the parser."
);

const a;
const b;
--a;
--b;

shouldBe("a", "undefined");

function f()
{
    const a;
    const b;
    --a;
    --b;

    return a;
}

shouldBe("f()", "undefined");

var successfullyParsed = true;

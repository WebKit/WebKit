function shouldBe(actual, expected)
{
    if (actual !== expected)
        abort();
}

let x = {
    get toString() {
        throw new Error('bad');
    }
};

import(x).then(abort, function (error) {
    shouldBe(String(error), `Error: bad`);
});

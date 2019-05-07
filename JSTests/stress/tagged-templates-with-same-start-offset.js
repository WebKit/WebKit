function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var sites = [];

function call(site)
{
    sites.push(site);
    return call;
}

call`Hello``New``World`

shouldBe(sites.length, 3);

shouldBe(sites[0] !== sites[1], true);
shouldBe(sites[0] !== sites[2], true);
shouldBe(sites[1] !== sites[2], true);

shouldBe(sites[0].length, 1);
shouldBe(sites[0][0], `Hello`);

shouldBe(sites[1].length, 1);
shouldBe(sites[1][0], `New`);

shouldBe(sites[2].length, 1);
shouldBe(sites[2][0], `World`);

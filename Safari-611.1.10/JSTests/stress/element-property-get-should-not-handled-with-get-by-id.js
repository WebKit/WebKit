(function () {
    function getOne(a)
    {
        return a['1'];
    }

    for (var i = 0; i < 36; ++i)
        getOne({2: true});

    if (!getOne({1: true}))
        throw new Error("OUT");

    for (var i = 0; i < 1e4; ++i)
        getOne({2: true});

    if (!getOne({1: true}))
        throw new Error("OUT");
}());

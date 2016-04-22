function opaqueDivBy2(a)
{
    return a / 2;
}
noInline(opaqueDivBy2);

function opaqueDivBy3(a)
{
    return a / 3;
}
noInline(opaqueDivBy3);

function opaqueDivBy4(a)
{
    return a / 4;
}
noInline(opaqueDivBy4);

function opaqueDivBySafeMaxMinusOne(a)
{
    // 2^1022
    return a / 44942328371557897693232629769725618340449424473557664318357520289433168951375240783177119330601884005280028469967848339414697442203604155623211857659868531094441973356216371319075554900311523529863270738021251442209537670585615720368478277635206809290837627671146574559986811484619929076208839082406056034304;
}
noInline(opaqueDivBySafeMaxMinusOne);

function opaqueDivBySafeMax(a)
{
    // 2^1023
    return a / 89884656743115795386465259539451236680898848947115328636715040578866337902750481566354238661203768010560056939935696678829394884407208311246423715319737062188883946712432742638151109800623047059726541476042502884419075341171231440736956555270413618581675255342293149119973622969239858152417678164812112068608;
}
noInline(opaqueDivBySafeMax);

function opaqueDivBySafeMaxPlusOne(a)
{
    // 2^1024
    return a / 179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624224137216;
}
noInline(opaqueDivBySafeMaxPlusOne);

function opaqueDivBySafeMin(a)
{
    // 2^-1022
    return a / (1 / 44942328371557897693232629769725618340449424473557664318357520289433168951375240783177119330601884005280028469967848339414697442203604155623211857659868531094441973356216371319075554900311523529863270738021251442209537670585615720368478277635206809290837627671146574559986811484619929076208839082406056034304);
}
noInline(opaqueDivBySafeMin);

function opaqueDivBySafeMinMinusOne(a)
{
    // 2^-1023
    return a / (1 / 89884656743115795386465259539451236680898848947115328636715040578866337902750481566354238661203768010560056939935696678829394884407208311246423715319737062188883946712432742638151109800623047059726541476042502884419075341171231440736956555270413618581675255342293149119973622969239858152417678164812112068608);
}
noInline(opaqueDivBySafeMinMinusOne);


for (let i = 0; i < 1e4; ++i) {
    let result = opaqueDivBy2(Math.PI);
    if (result !== 1.5707963267948966)
        throw "Failed opaqueDivBy2(Math.PI). Result = " + result;
    result = opaqueDivBy2(NaN);
    if (result === result)
        throw "Failed opaqueDivBy2(NaN). Result = " + result;
    result = opaqueDivBy2(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBy2(Infinity). Result = " + result;
    result = opaqueDivBy2(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBy2(-Infinity). Result = " + result;
    result = opaqueDivBy2(Math.E);
    if (result !== 1.3591409142295225)
        throw "Failed opaqueDivBy2(Math.E). Result = " + result;

    result = opaqueDivBy3(Math.PI);
    if (result !== 1.0471975511965976)
        throw "Failed opaqueDivBy3(Math.PI). Result = " + result;
    result = opaqueDivBy3(NaN);
    if (result === result)
        throw "Failed opaqueDivBy3(NaN). Result = " + result;
    result = opaqueDivBy3(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBy3(Infinity). Result = " + result;
    result = opaqueDivBy3(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBy3(-Infinity). Result = " + result;
    result = opaqueDivBy3(Math.E);
    if (result !== 0.9060939428196817)
        throw "Failed opaqueDivBy3(Math.E). Result = " + result;

    result = opaqueDivBy4(Math.PI);
    if (result !== 0.7853981633974483)
        throw "Failed opaqueDivBy4(Math.PI). Result = " + result;
    result = opaqueDivBy4(NaN);
    if (result === result)
        throw "Failed opaqueDivBy4(NaN). Result = " + result;
    result = opaqueDivBy4(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBy4(Infinity). Result = " + result;
    result = opaqueDivBy4(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBy4(-Infinity). Result = " + result;
    result = opaqueDivBy4(Math.E);
    if (result !== 0.6795704571147613)
        throw "Failed opaqueDivBy4(Math.E). Result = " + result;

    result = opaqueDivBySafeMaxMinusOne(Math.PI);
    if (result !== 6.990275687580919e-308)
        throw "Failed opaqueDivBySafeMaxMinusOne(Math.PI). Result = " + result;
    result = opaqueDivBySafeMaxMinusOne(NaN);
    if (result === result)
        throw "Failed opaqueDivBySafeMaxMinusOne(NaN). Result = " + result;
    result = opaqueDivBySafeMaxMinusOne(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMaxMinusOne(Infinity). Result = " + result;
    result = opaqueDivBySafeMaxMinusOne(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBySafeMaxMinusOne(-Infinity). Result = " + result;
    result = opaqueDivBySafeMaxMinusOne(Math.E);
    if (result !== 6.048377836559378e-308)
        throw "Failed opaqueDivBySafeMaxMinusOne(Math.E). Result = " + result;

    result = opaqueDivBySafeMax(Math.PI);
    if (result !== 3.4951378437904593e-308)
        throw "Failed opaqueDivBySafeMax(Math.PI). Result = " + result;
    result = opaqueDivBySafeMax(NaN);
    if (result === result)
        throw "Failed opaqueDivBySafeMax(NaN). Result = " + result;
    result = opaqueDivBySafeMax(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMax(Infinity). Result = " + result;
    result = opaqueDivBySafeMax(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBySafeMax(-Infinity). Result = " + result;
    result = opaqueDivBySafeMax(Math.E);
    if (result !== 3.024188918279689e-308)
        throw "Failed opaqueDivBySafeMax(Math.E). Result = " + result;

    result = opaqueDivBySafeMaxPlusOne(Math.PI);
    if (result !== 0)
        throw "Failed opaqueDivBySafeMaxPlusOne(Math.PI). Result = " + result;
    result = opaqueDivBySafeMaxPlusOne(NaN);
    if (result === result)
        throw "Failed opaqueDivBySafeMaxPlusOne(NaN). Result = " + result;
    result = opaqueDivBySafeMaxPlusOne(Infinity);
    if (result === result)
        throw "Failed opaqueDivBySafeMaxPlusOne(Infinity). Result = " + result;
    result = opaqueDivBySafeMaxPlusOne(-Infinity);
    if (result === result)
        throw "Failed opaqueDivBySafeMaxPlusOne(-Infinity). Result = " + result;
    result = opaqueDivBySafeMaxPlusOne(Math.E);
    if (result !== 0)
        throw "Failed opaqueDivBySafeMaxPlusOne(Math.E). Result = " + result;

    result = opaqueDivBySafeMin(Math.PI);
    if (result !== 1.4119048864730642e+308)
        throw "Failed opaqueDivBySafeMin(Math.PI). Result = " + result;
    result = opaqueDivBySafeMin(NaN);
    if (result === result)
        throw "Failed opaqueDivBySafeMin(NaN). Result = " + result;
    result = opaqueDivBySafeMin(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMin(Infinity). Result = " + result;
    result = opaqueDivBySafeMin(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBySafeMin(-Infinity). Result = " + result;
    result = opaqueDivBySafeMin(Math.E);
    if (result !== 1.2216591454104522e+308)
        throw "Failed opaqueDivBySafeMin(Math.E). Result = " + result;

    result = opaqueDivBySafeMinMinusOne(Math.PI);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMinMinusOne(Math.PI). Result = " + result;
    result = opaqueDivBySafeMinMinusOne(NaN);
    if (result === result)
        throw "Failed opaqueDivBySafeMinMinusOne(NaN). Result = " + result;
    result = opaqueDivBySafeMinMinusOne(Infinity);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMinMinusOne(Infinity). Result = " + result;
    result = opaqueDivBySafeMinMinusOne(-Infinity);
    if (result !== -Infinity)
        throw "Failed opaqueDivBySafeMinMinusOne(-Infinity). Result = " + result;
    result = opaqueDivBySafeMinMinusOne(Math.E);
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMinMinusOne(Math.E). Result = " + result;
}


// Check that we don't do anything crazy with crazy types.
for (let i = 0; i < 1e3; ++i) {
    let result = opaqueDivBy2();
    if (result === result)
        throw "Failed opaqueDivBy2()";
    result = opaqueDivBy4(null);
    if (result !== 0)
        throw "Failed opaqueDivBy4(null)";
    result = opaqueDivBySafeMaxMinusOne("WebKit!");
    if (result === result)
        throw "Failed opaqueDivBy4(null)";
    result = opaqueDivBySafeMin("");
    if (result !== 0)
        throw "Failed opaqueDivBySafeMin('')";
    try {
        result = opaqueDivBy2(Symbol());
        throw "Failed opaqueDivBy2(Symbol())";
    } catch (exception) {
        if (exception != "TypeError: Type error")
            throw "Wrong exception: " + exception;
    }
    result = opaqueDivBy4(true);
    if (result !== 0.25)
        throw "Failed opaqueDivBy4(true)";
    result = opaqueDivBySafeMaxMinusOne(false);
    if (result !== 0)
        throw "Failed opaqueDivBySafeMaxMinusOne(false)";
    result = opaqueDivBySafeMin({ valueOf: function() { return 42; }});
    if (result !== Infinity)
        throw "Failed opaqueDivBySafeMin({ valueOf: function() { return 42; }})";
}

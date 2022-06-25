let allRegExpFlags = "dgimsuy";
let allRegExpProperties = [ 'hasIndices', 'global', 'ignoreCase', 'multiline', 'dotAll', 'unicode', 'sticky'];
const numFlags = allRegExpFlags.length;
const numVariations = 2 ** numFlags;

function flagsFromVariation(variation)
{
    let flags = "";

    for (let i = 0; i < numFlags; i++)
        if (variation & (2 ** i))
            flags = flags + allRegExpFlags[i];

    return flags;
}

function setPropertiesForVariation(variation, o)
{
    for (let i = 0; i < numFlags; i++)
        if (variation & (2 ** i))
            o[allRegExpProperties[i]] = true;

    return o;
}

function missingPropertiesForVariation(variation, o)
{
    let missingProperties = [];

    for (let i = 0; i < numFlags; i++)
        if (variation & (2 ** i) && !o[allRegExpProperties[i]])
            missingProperties.push(allRegExpProperties[i]);

    return missingProperties;
}

var get = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get

function test()
{
    for (let variation = 0; variation < numVariations; ++variation) {
        let flags = flagsFromVariation(variation);

        let r = new RegExp("foo", flags);

        let missingProperties = missingPropertiesForVariation(variation, r);
        if (missingProperties.length)
            throw "RegExp " + r.toString() + " missing properties: " + missingProperties;

        r = setPropertiesForVariation(variation, {});

        let flagsSet = get.call(r);

        if (flagsSet != flags)
            throw "RegExp with flags: \"" + flags + "\" should have properties: " + missingPropertiesForVariation(variation, {});
    }
}

test();


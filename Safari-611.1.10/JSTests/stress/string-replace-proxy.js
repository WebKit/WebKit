function assert(assertion) {
    if (typeof assertion != "string")
        throw new Error("Invalid assertion.");

    let result = eval(assertion);

    if (!result)
        throw new Error("Bad assertion: " + assertion);
}

let calls = 0;
let getSet = [];

function resetTracking()
{
    calls = 0;
    getSet = [];
}

let getSetProxyReplace = new Proxy(
    {
        replace: function(string, search, replaceWith)
        {
            calls++;
            return string.replace(search, replaceWith);
        }
    }, {
        get: function(o, k)
        {
            getSet.push(k);
            return o[k];
        },
        set: function(o, k, v)
        {
            getSet.push(k);
            o[k] = v;
        }
    });

resetTracking();
let replaceResult = getSetProxyReplace.replace("This is a test", / /g, "_");
assert('replaceResult == "This_is_a_test"');
assert('calls === 1')
assert('getSet == "replace"');

resetTracking();
replaceResult = getSetProxyReplace.replace("This is a test", " ", "_");
assert('replaceResult == "This_is a test"');
assert('calls === 1')
assert('getSet == "replace"');

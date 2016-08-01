function assert(assertion) {
    if (typeof assertion != "string")
        throw new Error("Invalid assertion.");

    let result = eval(assertion);

    if (!result)
        throw new Error("Bad assertion: " + assertion);
}

let get = [];
let set = [];
let getSet = [];

function resetTracking()
{
    get = [];
    set = [];
    getSet = [];
}

let getProxyNullExec = new Proxy({
        exec: function()
        {
            return null;
        }
    }, {
        get: function(o, k)
        {
            get.push(k); return o[k];
        }
    });

resetTracking();
RegExp.prototype[Symbol.match].call(getProxyNullExec);
assert('get == "global,exec"');

let getSetProxyNullExec = new Proxy(
    {
        exec: function()
        {
            return null;
        }
    }, {
        get: function(o, k)
        {
            get.push(k);
            getSet.push(k);
            return o[k];
        },
        set: function(o, k, v)
        {
            set.push(k);
            getSet.push(k);
            o[k] = v;
        }
    });

getSetProxyNullExec.global = true;

resetTracking();
RegExp.prototype[Symbol.match].call(getSetProxyNullExec);
assert('get == "global,unicode,exec"');
assert('set == "lastIndex"');
assert('getSet == "global,unicode,lastIndex,exec"');

let regExpGlobal_s = new RegExp("s", "g");
let getSetProxyMatches_s = new Proxy(
    {
        exec: function(string)
        {
            return regExpGlobal_s.exec(string);
        }
    }, {
        get: function(o, k)
        {
            get.push(k);
            getSet.push(k);
            return o[k];
        },
        set: function(o, k, v)
        {
            set.push(k);
            getSet.push(k);
            o[k] = v;
        }
    });

getSetProxyMatches_s.global = true;
resetTracking();
let matchResult = RegExp.prototype[Symbol.match].call(getSetProxyMatches_s, "This is a test");
assert('matchResult == "s,s,s"');
assert('get == "global,unicode,exec,exec,exec,exec"');
assert('set == "lastIndex"');
assert('getSet == "global,unicode,lastIndex,exec,exec,exec,exec"');

let regExpGlobal_tx_Greedy = new RegExp("[tx]*", "g");
let getSetProxyMatches_tx_Greedy = new Proxy(
    {
        exec: function(string)
        {
            return regExpGlobal_tx_Greedy.exec(string);
        }
    }, {
        get: function(o, k)
        {
            get.push(k);
            getSet.push(k);
            if (k.toString() == "lastIndex")
                return regExpGlobal_tx_Greedy.lastIndex;
            return o[k];
        },
        set: function(o, k, v)
        {
            set.push(k);
            getSet.push(k);
            if (k.toString() == "lastIndex")
                regExpGlobal_tx_Greedy.lastIndex = v;
            o[k] = v;
        }
    });

getSetProxyMatches_tx_Greedy.global = true;

resetTracking();
matchResult = RegExp.prototype[Symbol.match].call(getSetProxyMatches_tx_Greedy, "testing");
assert('matchResult == "t,,,t,,,,"');
assert('get == "global,unicode,exec,exec,lastIndex,exec,lastIndex,exec,exec,lastIndex,exec,lastIndex,exec,lastIndex,exec,lastIndex,exec"');
assert('set == "lastIndex,lastIndex,lastIndex,lastIndex,lastIndex,lastIndex,lastIndex"');
assert('getSet == "global,unicode,lastIndex,exec,exec,lastIndex,lastIndex,exec,lastIndex,lastIndex,exec,exec,lastIndex,lastIndex,exec,lastIndex,lastIndex,exec,lastIndex,lastIndex,exec,lastIndex,lastIndex,exec"');

let regExpGlobalUnicode_digit_nonGreedy = new RegExp("\\d{0,1}", "gu");
let getSetProxyMatchesUnicode_digit_nonGreedy = new Proxy(
    {
        exec: function(string)
        {
            return regExpGlobalUnicode_digit_nonGreedy.exec(string);
        }
    }, {
        get: function(o, k)
        {
            get.push(k);
            getSet.push(k);
            if (k.toString() == "lastIndex")
                return regExpGlobalUnicode_digit_nonGreedy.lastIndex;
            return o[k];
        },
        set: function(o, k, v)
        {
            set.push(k);
            getSet.push(k);
            if (k.toString() == "lastIndex")
                regExpGlobalUnicode_digit_nonGreedy.lastIndex = v;
            o[k] = v;
        }
    });

getSetProxyMatchesUnicode_digit_nonGreedy.global = true;
getSetProxyMatchesUnicode_digit_nonGreedy.unicode = true;

resetTracking();
matchResult = RegExp.prototype[Symbol.match].call(getSetProxyMatchesUnicode_digit_nonGreedy, "12X3\u{10400}4");
assert('matchResult == "1,2,,3,,4,"');
assert('get == "global,unicode,exec,exec,exec,lastIndex,exec,exec,lastIndex,exec,exec,lastIndex,exec"');
assert('set == "lastIndex,lastIndex,lastIndex,lastIndex"');
assert('getSet == "global,unicode,lastIndex,exec,exec,exec,lastIndex,lastIndex,exec,exec,lastIndex,lastIndex,exec,exec,lastIndex,lastIndex,exec"');

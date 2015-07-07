
function createTests() {
    var result = [];
    result.push(function(jsonObject){
        return jsonObject.parse();
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('1');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('-1');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('Infinity');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('NaN');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('null');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('undefined');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{}');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('({})');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{a}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{a:}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{a:5}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{a:5,}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a"}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":5}');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('{"__proto__":5}');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":5,}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":5,,}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":5,"a",}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{"a":(5,"a"),}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('[]');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('[1]');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('[1,]');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('[1,2]');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('[1,2,,]');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('[1,2,,4]');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('""');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"\'"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\z"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\\z"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\\\z"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\tz"');
    });
    result[result.length - 1].throws = true; // rfc4627 does not allow literal tab characters in JSON source
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\tz"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\nz"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\nz"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\rz"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\rz"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\/z"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\/z"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\bz"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\bz"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\rz"');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\rz"');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\uz"     ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0z"    ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u00z"   ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u000z"  ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0000z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u000Az" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u000az" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u000Gz" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u000gz" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u00A0z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u00a0z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u00G0z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u00g0z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0A00z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0a00z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0G00z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\u0g00z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\uA000z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\ua000z" ');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\uG000z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('"a\\ug000z" ');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('00');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('01');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('0.a');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('0x0');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('2e1.3');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('2e-+10');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('2e+-10');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('2e3e4');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('-01.0');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('-01');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('-01.a');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('1.e1');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{/* block comments are not allowed */}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('{// line comments are not allowed \n}');
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        return jsonObject.parse('true');
    });
    result.push(function(jsonObject){
        return jsonObject.parse('false');
    });
    var simpleArray = ['a', 'b', 'c'];
    var simpleObject = {a:"1", b:"2", c:"3"};
    var complexArray = ['a', 'b', 'c',,,simpleObject, simpleArray, [simpleObject,simpleArray]];
    var complexObject = {a:"1", b:"2", c:"3", d:4.5e10, g: 0.45e-5, h: 0.0, i: 0, j:.5, k:0., l:-0, m:-0.0, n:-0., o:-.5, p:-0.45e-10, q:-4.5e10, e:null, "":12, f: simpleArray, array: complexArray};
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject));
    });
    result[result.length - 1].expected = JSON.stringify(simpleObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject));
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject));
    });
    result[result.length - 1].expected = JSON.stringify(complexObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject,null,100));
    });
    result[result.length - 1].expected = JSON.stringify(simpleObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null,100));
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null,100));
    });
    result[result.length - 1].expected = JSON.stringify(complexObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject,null," "));
    });
    result[result.length - 1].expected = JSON.stringify(simpleObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null," "));
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null," "));
    });
    result[result.length - 1].expected = JSON.stringify(complexObject);
    
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject,null,"\t"));
    });
    result[result.length - 1].expected = JSON.stringify(simpleObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null,"\t"));
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null,"\t"));
    });
    result[result.length - 1].expected = JSON.stringify(complexObject);
    
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject,null,"\n"));
    });
    result[result.length - 1].expected = JSON.stringify(simpleObject);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject,null,"\n"));
    });
    result[result.length - 1].expected = JSON.stringify(complexObject);
    function log(key, value) {
        var o = {};
        o[key] = value;
        o.keyType = typeof key;
        return o;
    }
    result.push(function(jsonObject){
        return jsonObject.parse("true", log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse("false", log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse("null", log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse("1", log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse("1.5", log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse('"a string"', log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleArray), log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexArray), log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(simpleObject), log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(complexObject), log);
    });
    result.push(function(jsonObject){
        return jsonObject.parse('{"__proto__":{"a":5}}', log);
    });
    var logOrderString;
    function logOrder(key, value) {
        logOrderString += key +":"+JSON.stringify(value);
        return null;
    }
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse("true", logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse("false", logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse("null", logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse("1", logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse("1.5", logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse('"a string"', logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(simpleArray), logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(complexArray), logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(simpleObject), logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(complexObject), logOrder);
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse("true", logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse("false", logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse("null", logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse("1", logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse("1.5", logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse('"a string"', logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse(JSON.stringify(simpleArray), logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse(JSON.stringify(complexArray), logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse(JSON.stringify(simpleObject), logOrder);
        return logOrderString;
    });
    result.push(function(jsonObject){
        logOrderString = "";
        jsonObject.parse(JSON.stringify(complexObject), logOrder);
        return logOrderString;
    });
    var callCount = 0;
    function throwAfterFifthCall(key, value) {
        logOrder(key, value);
        if (++callCount > 5)
            throw "from reviver";
        return null;
    }
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(complexArray), throwAfterFifthCall);
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(simpleObject), throwAfterFifthCall);
    });
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        return jsonObject.parse(JSON.stringify(complexObject), throwAfterFifthCall);
    });
    result[result.length - 1].throws = true;
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        try { jsonObject.parse(JSON.stringify(complexArray), throwAfterFifthCall); } catch (e) {}
        return logOrderString;
    });
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        try { jsonObject.parse(JSON.stringify(simpleObject), throwAfterFifthCall); } catch (e) {}
        return logOrderString;
    });
    result.push(function(jsonObject){
        callCount = 0;
        logOrderString = "";
        try { jsonObject.parse(JSON.stringify(complexObject), throwAfterFifthCall); } catch (e) {}
        return logOrderString;
    });
    var unicode = "";
    for (var i = 0; i < 1<<16; i++)
        unicode += String.fromCharCode(i);
    result.push(function(jsonObject){
        return jsonObject.parse(JSON.stringify(unicode));
    });
    result[result.length - 1].unstringifiedExpected = unicode;
    return result;
}
var tests = createTests();
for (var i = 0; i < tests.length; i++) {
    try {
        debug(tests[i]);
        if (tests[i].throws) {
            shouldThrow('tests[i](nativeJSON)');
            try {
                var threw = false;
                tests[i](JSON);
            } catch(e) {
                var threw = true;
            }
            if (!threw)
                debug("json2.js did not throw for a test we expect to throw.");
        } else if (tests[i].expected)
            try { shouldBe('JSON.stringify(tests[i](nativeJSON))',  "tests[i].expected") } catch(e) { debug("threw - " + e)}
        else if (tests[i].unstringifiedExpected)
            try { shouldBe('tests[i](nativeJSON)',  "tests[i].unstringifiedExpected") } catch(e) { debug("threw - " + e)}
        else
            try { shouldBe('JSON.stringify(tests[i](nativeJSON))',  'JSON.stringify(tests[i](JSON))') } catch(e) { debug("threw - " + e) };
    }catch(e){
        debug(e);
    }
}


description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=95815">'
);

function testPostIncConstVarWithIgnoredResult()
{
    var okay = false;
    const a = {
        valueOf: (function(){
            okay = true;
        })
    };

    a++;

    return okay;
}

function testPreIncConstVarWithIgnoredResult()
{
    var okay = false;
    const a = {
        valueOf: (function(){
            okay = true;
        })
    };

    ++a;

    return okay;
}

function testPreIncConstVarWithAssign()
{
    var okay = false;
    var x = 42;
    const a = {
        valueOf: (function(){
            throw x == 42;
        })
    };

    try {
        x = ++a;
    } catch (e) {
        okay = e
    };

    return okay;
}

shouldBeTrue('testPostIncConstVarWithIgnoredResult()');
shouldBeTrue('testPreIncConstVarWithIgnoredResult()');
shouldBeTrue('testPreIncConstVarWithAssign()');

successfullyParsed = true;

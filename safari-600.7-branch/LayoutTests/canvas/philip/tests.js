function _valToString(val)
{
    if (val === undefined || val === null)
        return '[' + typeof(val) + ']';
    return val.toString() + '[' + typeof(val) + ']';
}

var _failed = false;
var _asserted = false;

function _warn(text)
{
    document.getElementById('d').appendChild(document.createElement('li')).appendChild(document.createTextNode(text));
}

function _fail(text)
{
    _warn(text);
    _failed = true;
}

function _assert(cond, text)
{
    _asserted = true;
    if (! cond)
        _fail('Failed assertion: ' + text);
}

function _assertSame(a, b, text_a, text_b)
{
    _asserted = true;
    if (a !== b)
        _fail('Failed assertion ' + text_a + ' === ' + text_b +
                ' (got ' + _valToString(a) + ', expected ' + _valToString(b) + ')');
}

function _assertDifferent(a, b, text_a, text_b)
{
    _asserted = true;
    if (a === b)
        _fail('Failed assertion ' + text_a + ' !== ' + text_b +
                ' (got ' + _valToString(a) + ', expected not ' + _valToString(b) + ')');
}

function _assertEqual(a, b, text_a, text_b)
{
    _asserted = true;
    if (a != b)
        _fail('Failed assertion ' + text_a + ' == ' + text_b +
                ' (got ' + _valToString(a) + ', expected ' + _valToString(b) + ')');
}

function _assertMatch(a, b, text_a, text_b)
{
    _asserted = true;
    if (! a.match(b))
        _fail('Failed assertion ' + text_a + ' matches ' + text_b +
                ' (got ' + _valToString(a) + ')');
}


var _manual_check = false;

function _requireManualCheck()
{
    _manual_check = true;
}

function _crash()
{
    _fail('Aborted due to predicted crash');
}

function _getPixel(canvas, x,y)
{
    try
    {
        var ctx = canvas.getContext('2d');
        var imgdata = ctx.getImageData(x, y, 1, 1);
        return [ imgdata.data[0], imgdata.data[1], imgdata.data[2], imgdata.data[3] ];
    }
    catch (e)
    {
        // probably a security exception caused by having drawn
        // data: URLs onto the canvas
        _manual_check = true;
        return undefined;
    }
}

function _assertPixel(canvas, x,y, r,g,b,a, pos, colour)
{
    _asserted = true;
    var c = _getPixel(canvas, x,y);
    if (c && ! (c[0] == r && c[1] == g && c[2] == b && c[3] == a))
        _fail('Failed assertion: got pixel [' + c + '] at ('+x+','+y+'), expected ['+r+','+g+','+b+','+a+']');
}

function _assertPixelApprox(canvas, x,y, r,g,b,a, pos, colour, tolerance)
{
    _asserted = true;
    var c = _getPixel(canvas, x,y);
    if (c)
    {
        var diff = Math.max(Math.abs(c[0]-r), Math.abs(c[1]-g), Math.abs(c[2]-b), Math.abs(c[3]-a));
        if (diff > tolerance)
            _fail('Failed assertion: got pixel [' + c + '] at ('+x+','+y+'), expected ['+r+','+g+','+b+','+a+'] +/- '+tolerance);
    }
}

function _addTest(test)
{
    var deferred = false;
    window.deferTest = function () { deferred = true; };
    function endTest()
    {
        if (_failed) // test failed
        {
            document.documentElement.className += ' fail';
            window._testStatus = ['fail', document.getElementById('d').innerHTML];
        }
        else if (_manual_check || !_asserted)
        { // test case explicitly asked for a manual check, or no automatic assertions were performed
            document.getElementById('d').innerHTML += '<li>Cannot automatically verify result';
            document.documentElement.className += ' needs_check';
            window._testStatus = ['check', document.getElementById('d').innerHTML];
        }
        else // test succeeded
        {
            document.getElementById('d').innerHTML += '<li>Passed';
            document.documentElement.className += ' pass';
            window._testStatus = ['pass', document.getElementById('d').innerHTML];
        }
        if (window.testRunner)
            testRunner.notifyDone();
    };
    window.endTest = endTest;
    window.wrapFunction = function (f)
    {
        return function()
        {
            try
            {
                f.apply(null, arguments);
            }
            catch (e)
            {
                _fail('Aborted with exception: ' + e.message);
            }
            endTest();
        };
    };

    window.onload = function ()
    {
        if (window.testRunner) {
            testRunner.dumpAsText();
            testRunner.waitUntilDone();
        }
        try
        {
            var canvas = document.getElementById('c');
            var ctx = canvas.getContext('2d');
            test(canvas, ctx);
        }
        catch (e)
        {
            _fail('Aborted with exception: ' + e.message);
            deferred = false; // cancel any deference
        }

        if (! deferred)
            endTest();
    };
}


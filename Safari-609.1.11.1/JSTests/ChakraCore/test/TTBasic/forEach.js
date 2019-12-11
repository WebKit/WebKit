//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = { foo: 3, bar: null };
x.foo2 = 6;

var y = { foo: 5, bar: 'bar', baz: null };
delete y.bar;
y.bar2 = 'bar2'

var z = { foo: 10, bar: 'bar' };
delete z.bar;
z.baz = 'baz'
z.bar = 'bar'

var xo = [];
for(var xname in x)
{
    xo.push(xname);
}

var yo = [];
for(var yname in y)
{
    yo.push(yname);
}

var zo = [];
for(var zname in z)
{
    zo.push(zname);
}

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    var idx = -1;
    
    idx = 0;
    for(var xname in x)
    {
        telemetryLog(`xname: ${xname}`, true); //xo[idx]
        idx++;    
    }
    
    idx = 0;
    for(var yname in y)
    {
        telemetryLog(`yname: ${yname}`, true); //yo[idx]
        idx++;    
    }
    
    idx = 0;
    for(var zname in z)
    {
        telemetryLog(`zname: ${zname}`, true); //zo[idx]
        idx++;    
    }
}
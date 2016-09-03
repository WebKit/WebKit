//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var handler1 = {
    get: function(target, key)
    {
        return key in target ? target[key] : 'Not Found';
    }
};

var handler2 = {
    get: function(target, key)
    {
        return "[[" + key + "]]";;
    }
};

var p = new Proxy({}, handler1);
p.a = 1;

var revocable = Proxy.revocable({}, handler2);
var proxy = revocable.proxy;

var revocableDone = Proxy.revocable({}, handler2);
var proxyDone = revocableDone.proxy;

revocableDone.revoke();

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    var threw = false;
        
    telemetryLog(`p.a: ${p.a}`, true); //1);
    telemetryLog(`p.b: ${p.b}`, true); //Not Found

    try
    {
        proxyDone.foo;
    }
    catch(e)
    {
        threw = true;
    }
    telemetryLog(`proxyDone.foo: ${threw}`, true); //true

    telemetryLog(`proxy.foo: ${proxy.foo}`, true); //[[foo]]

    revocable.revoke();
    try
    {
        proxy.foo;
    }
    catch(e)
    {
        threw = true;
    }
    telemetryLog(`proxy.foo (after revoke): ${threw}`, true); //true
}
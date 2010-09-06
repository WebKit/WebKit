/* 
Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

var WebKitDetect = {  };

// If the user agent is WebKit, returns true. Otherwise, returns false.
WebKitDetect.isWebKit = function isWebKit()
{
    return RegExp(" AppleWebKit/").test(navigator.userAgent);
}

// If the user agent is WebKit, returns an array of numbers matching the "." separated 
// fields in the WebKit version number, with an "isNightlyBuild" property specifying
// whether the user agent is a WebKit nightly build. Otherwise, returns null.
//
// Example: 418.10.1 => [ 418, 10, 1 ] isNightlyBuild: false
WebKitDetect.version = function version() 
{
    /* Some example strings: 
            Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3
            Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/420+ (KHTML, like Gecko) Safari/521.32
     */
     
    // grab (AppleWebKit/)(xxx.x.x)
    var webKitFields = RegExp("( AppleWebKit/)([^ ]+)").exec(navigator.userAgent);
    if (!webKitFields || webKitFields.length < 3)
        return null;
    var versionString = webKitFields[2];

    var isNightlyBuild = versionString.indexOf("+") != -1;

    // Remove '+' or any other stray characters
    var invalidCharacter = RegExp("[^\\.0-9]").exec(versionString);
    if (invalidCharacter)
        versionString = versionString.slice(0, invalidCharacter.index);
    
    var version = versionString.split(".");
    version.isNightlyBuild = isNightlyBuild;
    return version;
}

// If the user agent is a WebKit version greater than or equal to the version specified
// in the string minimumString, returns true. Returns false otherwise. minimumString 
// defaults to "".
//
// Example usage: WebKitDetect.versionIsAtLeast("418.10.1")
WebKitDetect.versionIsAtLeast = function versionIsAtLeast(minimumString)
{
    function toIntOrZero(s) 
    {
        var toInt = parseInt(s);
        return isNaN(toInt) ? 0 : toInt;
    }

    if (minimumString === undefined)
        minimumString = "";
    
    var minimum = minimumString.split(".");
    var version = WebKitDetect.version();

    if (!version)
        return false;
        
    if (version.isNightlyBuild)
        return true;

    for (var i = 0; i < minimum.length; i++) {
        var versionField = toIntOrZero(version[i]);
        var minimumField = toIntOrZero(minimum[i]);
        
        if (versionField > minimumField)
            return true;
        if (versionField < minimumField)
            return false;
    }

    return true;
}

WebKitDetect.isMobile = function isMobile()
{
    return WebKitDetect.isWebKit() && RegExp(" Mobile\\b").test(navigator.userAgent);
}

WebKitDetect.mobileDevice = function mobileDevice()
{
    if (!WebKitDetect.isMobile())
        return null;
        
    var fields = RegExp("(Mozilla/5.0 \\()([^;]+)").exec(navigator.userAgent);
    if (!fields || fields.length < 3)
        return null;
    return fields[2];
}

// Example: 1C28 => [ 1, C, 28 ]
WebKitDetect._mobileVersion = function _mobileVersion(versionString)
{
    var fields = RegExp("([0-9]+)([A-Z]+)([0-9]+)").exec(versionString);
    if (!fields || fields.length != 4)
        return null;
    return [ fields[1], fields[2], fields[3] ];
}

WebKitDetect.mobileVersion = function mobileVersion()
{
    // grab (Mobile/)(nxnnn)
    var fields = RegExp("( Mobile/)([^ ]+)").exec(navigator.userAgent);
    if (!fields || fields.length < 3)
        return null;
    var versionString = fields[2];
    
    return WebKitDetect._mobileVersion(versionString);
}

WebKitDetect.mobileVersionIsAtLeast = function mobileVersionIsAtLeast(minimumString)
{
    function toIntOrZero(s) 
    {
        var toInt = parseInt(s);
        return isNaN(toInt) ? 0 : toInt;
    }

    if (minimumString === undefined)
        minimumString = "";

    var minimum = WebKitDetect._mobileVersion(minimumString);
    var version = WebKitDetect.mobileVersion();

    if (!version)
        return false;
        
    var majorVersInt = toIntOrZero(version[0]);
    var majorMinInt = toIntOrZero(minimum[0]);
    if (majorVersInt > majorMinInt)
        return true;
    if (majorVersInt < majorMinInt)
        return false;
    
    var majorVersAlpha = version[1];
    var majorMinAlpha = minimum[1];
    if (majorVersAlpha > majorMinAlpha)
        return true;
    if (majorVersAlpha < majorMinAlpha)
        return false;
    
    var minorVersInt = toIntOrZero(version[2]);
    var minorMinInt = toIntOrZero(minimum[2]);
    if (minorVersInt > minorMinInt)
        return true;
    if (minorVersInt < minorMinInt)
        return false;
    
    return true;
}



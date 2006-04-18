/* 
Copyright (C) 2006 Joost de Valk, http://www.joostdevalk.nl/.  All rights reserved.
Copyright (C) 2006 Mark Rowe, http://bdash.net.nz/.  All rights reserved.
Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.

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

Script used for recognizing Safari / Shiira / WebKit. 
A matrix of WebKit versions and OS X versions can be found at:
http://developer.apple.com/internet/safari/uamatrix.html .
*/

function parse_webkit_version(version)
{
  var bits = version.split(".");
  var is_nightly = (version[version.length - 1] == "+");
  if (is_nightly) {
    var minor = "+";
  } else {
    var minor = parseInt(bits[1]);
    // If minor is Not a Number (NaN) return an empty string
    if (isNaN(minor)) {
      minor = "";
    }
  }
  return {major: parseInt(bits[0]), minor: minor, is_nightly: is_nightly};
}

function get_webkit_version()
{
  var browser = "";
  
  // Check for Safari
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) \\(KHTML, like Gecko\\) Safari/(.*)");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "Safari "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);    
  } 
  
  // Check for Shiira
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) \\(KHTML, like Gecko\\) Shiira/(.*) Safari/(.*)");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "Shiira "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);
  } 

  // Check for OmniWeb 4 or 5
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) \\(KHTML, like Gecko\\) OmniWeb/v(.*) ");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "OmniWeb "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);
  }

  // Check for OmniWeb 5.1 and up
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) \\(KHTML, like Gecko, Safari\\) OmniWeb/v(.*) ");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "OmniWeb "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);
  }

  // Check for NetNewsWire 2 and higher
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) (KHTML, like Gecko) NetNewsWire/(.*)");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "NetNewsWire "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);
  }

  // Check for RealPlayer
  var regex = new RegExp("Mozilla/5.0 \\(.*\\) AppleWebKit/(.*) (KHTML, like Gecko) RealPlayer/(.*)");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var browser = "RealPlayer "+matches[2];
    var webkit_version = parse_webkit_version(matches[1]);
  }

  return {major: webkit_version['major'], minor: webkit_version['minor'], is_nightly: webkit_version['is_nightly'], browser: browser};
}  

// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//         * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//         * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


var history = history || {};

(function() {

history.validateParameter = function(state, key, value, validateFn)
{
    if (validateFn())
        state[key] = value;
    else
        console.log(key + ' value is not valid: ' + value);
}

history.isTreeMap = function()
{
    return string.endsWith(window.location.pathname, 'treemap.html');
}

// TODO(jparent): Make private once callers move here.
history.queryHashAsMap = function()
{
    var hash = window.location.hash;
    var paramsList = hash ? hash.substring(1).split('&') : [];
    var paramsMap = {};
    var invalidKeys = [];
    for (var i = 0; i < paramsList.length; i++) {
        var thisParam = paramsList[i].split('=');
        if (thisParam.length != 2) {
            console.log('Invalid query parameter: ' + paramsList[i]);
            continue;
        }

        paramsMap[thisParam[0]] = decodeURIComponent(thisParam[1]);
    }

    // FIXME: remove support for mapping from the master parameter to the group
    // one once the waterfall starts to pass in the builder name instead.
    if (paramsMap.master) {
        paramsMap.group = LEGACY_BUILDER_MASTERS_TO_GROUPS[paramsMap.master];
        if (!paramsMap.group)
            console.log('ERROR: Unknown master name: ' + paramsMap.master);
        window.location.hash = window.location.hash.replace('master=' + paramsMap.master, 'group=' + paramsMap.group);
        delete paramsMap.master;
    }

    return paramsMap;
}

// TODO(jparent): Make private once callers move here.
history.diffStates = function(oldState, newState)
{
    // If there is no old state, everything in the current state is new.
    if (!oldState)
        return newState;

    var changedParams = {};
    for (curKey in newState) {
        var oldVal = oldState[curKey];
        var newVal = newState[curKey];
        // Add new keys or changed values.
        if (!oldVal || oldVal != newVal)
            changedParams[curKey] = newVal;
    }
    return changedParams;
}

// TODO(jparent): Make private once callers move here.
history.fillMissingValues = function(to, from)
{
    for (var state in from) {
        if (!(state in to))
            to[state] = from[state];
    }
}

})();
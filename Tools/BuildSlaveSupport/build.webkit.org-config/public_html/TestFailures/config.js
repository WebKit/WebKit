/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var config = config || {};

(function() {

config.kBuilders = [
    'Webkit Win',
    'Webkit Vista',
    'Webkit Win7',
    'Webkit Win (dbg)(1)',
    'Webkit Win (dbg)(2)',
    'Webkit Linux',
    'Webkit Linux 32',
    'Webkit Linux (dbg)(1)',
    'Webkit Linux (dbg)(2)',
    'Webkit Mac10.5 (CG)',
    'Webkit Mac10.5 (CG)(dbg)(1)',
    'Webkit Mac10.5 (CG)(dbg)(2)',
    'Webkit Mac10.6 (CG)',
    'Webkit Mac10.6 (CG)(dbg)',
];

config.kBuildersThatOnlyCompile = [
    'Webkit Win Builder',
    'Webkit Win Builder (dbg)',
    // FIXME: Where is the Linux Builder?
    'Webkit Mac Builder',
    'Webkit Mac Builder (dbg)',
    'Win Builder',
    'Mac Clang Builder (dbg)',
];

config.kTracURL = 'http://trac.webkit.org';
config.kBugzillaURL = 'https://bugs.webkit.org';
config.kLocalServerURL = 'http://127.0.0.1:8127';

config.kRevisionAttr = 'data-revision';
config.kTestNameAttr = 'data-test-name';
config.kBuilderNameAttr = 'data-builder-name';
config.kFailureCountAttr = 'data-failure-count';
config.kFailureTypesAttr = 'data-failure-types';
config.kInfobarTypeAttr = 'data-infobar-type';

var kTenMinutesInMilliseconds = 10 * 60 * 1000;
config.kUpdateFrequency = kTenMinutesInMilliseconds;

})();

// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

class Expectations
{
    static colorMap() {
        const computedStyle = getComputedStyle(document.body);
        return {
            success: computedStyle.getPropertyValue('--greenLight').trim(),
            warning: computedStyle.getPropertyValue('--orangeDark').trim(),
            failed: computedStyle.getPropertyValue('--redLight').trim(),
            image: computedStyle.getPropertyValue('--blueLight').trim(),
            timedout: computedStyle.getPropertyValue('--orangeLight').trim(),
            crashed: computedStyle.getPropertyValue('--purpleLight').trim(),
        };
    }

    static stringToStateId(string) {
        return string in Expectations.stateToIdMap ? Expectations.stateToIdMap[string] : string;
    }
    static typeForId(string) {
        const id = Expectations.stringToStateId(string);
        let result = 'success';
        Expectations.failureTypes.forEach(type => {
            const idForType = Expectations.stringToStateId(Expectations.failureTypeMap[type]);
            if (id <= idForType)
                result = type;
        });
        return result;
    }
    static symbolForId(string) {
        return Expectations.symbolMap[Expectations.typeForId(string)];
    }
    static colorForId(string) {
        return Expectations.colorMap()[Expectations.typeForId(string)];
    }

    static unexpectedResults(results, expectations)
    {
        const unexpectedSet = new Set(results.split('.'));
        const expectedSet = new Set(expectations.split(' '));

        if (expectedSet.has('FAIL'))
            expectedSet.add('TEXT').add('AUDIO').add('IMAGE');

        for (const result of unexpectedSet) {
            if (expectedSet.has(result))
                unexpectedSet.delete(result);
        }

        let result = 'PASS';
        for (const candidate of unexpectedSet) {
            if (Expectations.stringToStateId(candidate) < Expectations.stringToStateId(result))
                result = candidate;
        }
        return result;
    }
}

Expectations.stateToIdMap = {
    CRASH: 0x00,
    TIMEOUT: 0x08,
    IMAGE: 0x10,
    AUDIO: 0x18,
    TEXT: 0x20,
    FAIL: 0x28,
    ERROR: 0x30,
    WARNING: 0x38,
    PASS: 0x40,
};
Expectations.failureTypes = ['warning', 'failed', 'image', 'timedout', 'crashed'];
Expectations.failureTypeMap = {
    warning: 'WARNING',
    failed: 'FAIL',
    image: 'IMAGE',
    timedout: 'TIMEOUT',
    crashed: 'CRASH',
}
const timeoutImage = new Image();
timeoutImage.src = '/library/icons/clock.svg';
timeoutImage.toString = function () {
    return this.outerHTML;
}
Expectations.symbolMap = {
    success: '✓',
    warning: '?',
    failed: '𝖷',
    image: 'I',
    timedout: timeoutImage,
    crashed: '!',
}

export {Expectations};

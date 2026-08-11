/*
 * Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>
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

(function () {
    'use strict';

    /* To bundle all code into browserified JS file, call `require` to the each test file explicitly;
     * requiring dependent files as string literals form to make them analyzable by browserify.
     * And export it to the global namespace as `promiseTests`.
     */
    global.promiseTests = {
        tests2_1_2: function () {
            return require('./promises-tests/lib/tests/2.1.2');
        },

        tests2_1_3: function () {
            return require('./promises-tests/lib/tests/2.1.3');
        },

        tests2_2_1: function () {
            return require('./promises-tests/lib/tests/2.2.1');
        },

        tests2_2_2: function () {
            return require('./promises-tests/lib/tests/2.2.2');
        },

        tests2_2_3: function () {
            return require('./promises-tests/lib/tests/2.2.3');
        },

        tests2_2_4: function () {
            return require('./promises-tests/lib/tests/2.2.4');
        },

        tests2_2_5: function () {
            return require('./promises-tests/lib/tests/2.2.5');
        },

        tests2_2_6: function () {
            return require('./promises-tests/lib/tests/2.2.6');
        },

        tests2_2_7: function () {
            return require('./promises-tests/lib/tests/2.2.7');
        },

        tests2_3_1: function () {
            return require('./promises-tests/lib/tests/2.3.1');
        },

        tests2_3_2: function () {
            return require('./promises-tests/lib/tests/2.3.2');
        },

        tests2_3_3: function () {
            return require('./promises-tests/lib/tests/2.3.3');
        },

        tests2_3_4: function () {
            return require('./promises-tests/lib/tests/2.3.4');
        }
    };
}());

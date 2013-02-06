/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockGrammarCheck.h"

#include "Platform/chromium/public/WebCString.h"
#include "Platform/chromium/public/WebString.h"
#include "TestCommon.h"
#include "WebTextCheckingResult.h"
#include <algorithm>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

bool MockGrammarCheck::checkGrammarOfString(const WebString& text, vector<WebTextCheckingResult>* results)
{
    WEBKIT_ASSERT(results);
    string16 stringText = text;
    if (find_if(stringText.begin(), stringText.end(), isASCIIAlpha) == stringText.end())
        return true;

    // Find matching grammatical errors from known ones. This function has to
    // check all errors because the given text may consist of two or more
    // sentences that have grammatical errors.
    static const struct {
        const char* text;
        int location;
        int length;
    } grammarErrors[] = {
        {"I have a issue.", 7, 1},
        {"I have an grape.", 7, 2},
        {"I have an kiwi.", 7, 2},
        {"I have an muscat.", 7, 2},
        {"You has the right.", 4, 3},
        {"apple orange zz.", 0, 16},
        {"apple zz orange.", 0, 16},
        {"apple,zz,orange.", 0, 16},
        {"orange,zz,apple.", 0, 16},
        {"the the adlj adaasj sdklj. there there", 0, 38},
        {"zz apple orange.", 0, 16},
    };
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(grammarErrors); ++i) {
        int offset = 0;
        string16 error(grammarErrors[i].text, grammarErrors[i].text + strlen(grammarErrors[i].text));
        while ((offset = stringText.find(error, offset)) != string16::npos) {
            results->push_back(WebTextCheckingResult(WebTextCheckingTypeGrammar, offset + grammarErrors[i].location, grammarErrors[i].length));
            offset += grammarErrors[i].length;
        }
    }
    return false;
}

}

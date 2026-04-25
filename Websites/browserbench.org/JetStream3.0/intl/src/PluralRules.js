/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
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

function* pluralRulesOptions() {
  const typeOptions = ["cardinal", "ordinal"];
  for (const locale of LOCALES) {
    for (const type of typeOptions) {
      yield { locale, type };
    }
  }
}

function runTest(verbose = false) {
  let lastResult;
  let totalLength = 0;
  const PLURAL_RULES_COUNT = 1000;
  for (const { locale, type } of shuffleOptions(pluralRulesOptions)) {
    if (verbose) {
      console.log(locale, type);
    }
    const formatter = new Intl.PluralRules(locale, { type });
    let i = 0;
    for (let value = 0; value < 4; value++) {
      lastResult = formatter.select(value);
      totalLength += lastResult.length;
      if (verbose) {
        console.log(value, lastResult);
      }
      i++;
    }
    for (; i < PLURAL_RULES_COUNT; i++) {
      const value = Math.floor(Math.random() * 1000);
      lastResult = formatter.select(value);
      totalLength += lastResult.length;
      if (verbose) {
        console.log(value, lastResult);
      }
    }
  }
  return { lastResult, totalLength, expectedMinLength: 244_000 };
}

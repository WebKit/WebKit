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

const LISTS = [
  ["One"],
  ["1", "2"],
  ["Motorcycle", "Bus", "Car"],
  LOCALES,
  new Array(100).fill(9).map((_, index) => index.toString()),
];

function* listOptions() {
  const styleOptions = ["long", "short", "narrow"];
  const typeOptions = ["conjunction", "disjunction", "unit"];
  for (const locale of LOCALES) {
    for (const style of styleOptions) {
      for (const type of typeOptions) {
        yield { locale, style, type };
      }
    }
  }
}

function runTest(verbose = false) {
  let lastResult;
  let totalLength = 0;
  const LIST_FORMAT_COUNT = 10;
  let listIndex = 0;
  for (const { locale, style, type } of shuffleOptions(listOptions)) {
    const options = { style, type };
    if (verbose) {
      console.log(locale, JSON.stringify(options));
    }
    const formatter = new Intl.ListFormat(locale, options);
    for (let i = 0; i < LIST_FORMAT_COUNT; i++) {
      const list = LISTS[listIndex % LISTS.length];
      listIndex++;
      lastResult = formatter.format(list);
      totalLength += lastResult.length;
      const formatPartsResult = formatter.formatToParts(list);
      if (verbose) {
        console.log(value, lastResult);
      }
      for (const part of formatPartsResult) {
        totalLength += part.value.length;
      }
    }
  }
  return { lastResult, totalLength, expectedMinLength: 506_000 };
}

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

const CURRENCIES = ["USD", "EUR", "JPY", "INR", "NGN"];

const NUMBER_UNITS = [
  "acre",
  "bit",
  "byte",
  "celsius",
  "centimeter",
  "day",
  "degree",
  "fahrenheit",
  "fluid-ounce",
  "foot",
  "gallon",
  "gigabit",
  "gigabyte",
  "gram",
  "hectare",
  "hour",
  "inch",
  "kilobit",
  "kilobyte",
  "kilogram",
  "kilometer",
  "liter",
  "megabit",
  "megabyte",
  "meter",
  "microsecond",
  "mile",
  "mile-scandinavian",
  "milliliter",
  "millimeter",
  "millisecond",
  "minute",
  "month",
  "nanosecond",
  "ounce",
  "percent",
  "petabyte",
  "pound",
  "second",
  "stone",
  "terabit",
  "terabyte",
  "week",
  "yard",
  "year",
];

function* numberFormatOptions() {
  const currencyDisplayOptions = ["symbol", "narrowSymbol", "code", "name"];
  const unitDisplayOptions = ["short", "long", "narrow"];

  for (const locale of LOCALES) {
    for (const currency of CURRENCIES) {
      for (const currencyDisplay of currencyDisplayOptions) {
        yield { locale, style: "currency", currency, currencyDisplay };
      }
    }
    for (const unit of NUMBER_UNITS.slice(0, 20)) {
      for (const unitDisplay of unitDisplayOptions) {
        yield { locale, style: "unit", unit, unitDisplay };
      }
    }
    yield { locale, style: "decimal" };
    yield { locale, style: "percent" };
  }
}

function runTest(verbose = false) {
  let lastResult;
  let totalLength = 0;
  const NUMBER_FORMAT_COUNT = 10;
  let counter = 1;
  for (const options of shuffleOptions(numberFormatOptions).slice(0, 200)) {
    const formatter = new Intl.NumberFormat(options.locale, options);
    if (verbose) {
      console.log(options.locale, JSON.stringify(options));
    }
    for (let i = 0; i < NUMBER_FORMAT_COUNT; i++) {
      counter += 599;
      const value = counter % 10_000;
      lastResult = formatter.format(value);
      if (verbose) {
        console.log(value, lastResult);
      }
      totalLength += lastResult.length;
      const formatPartsResult = formatter.formatToParts(value);
      for (const part of formatPartsResult) {
        totalLength += part.value.length;
      }
    }
  }
  return { lastResult, totalLength, expectedMinLength: 40_000 };
}

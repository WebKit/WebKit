// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Functions for normalizing fonts. Since the WOFF 2.0 decoder creates font
// files in normalized form, the WOFF 2.0 conversion is guaranteed to be
// lossless (in a bitwise sense) only for normalized font files.

#ifndef WOFF2_NORMALIZE_H_
#define WOFF2_NORMALIZE_H_

namespace woff2 {

struct Font;
struct FontCollection;

// Changes the offset fields of the table headers so that the data for the
// tables will be written in order of increasing tag values, without any gaps
// other than the 4-byte padding.
bool NormalizeOffsets(Font* font);

// Changes the checksum fields of the table headers and the checksum field of
// the head table so that it matches the current data.
bool FixChecksums(Font* font);

// Parses each of the glyphs in the font and writes them again to the glyf
// table in normalized form, as defined by the StoreGlyph() function. Changes
// the loca table accordigly.
bool NormalizeGlyphs(Font* font);

// Performs all of the normalization steps above.
bool NormalizeFont(Font* font);
bool NormalizeFontCollection(FontCollection* font_collection);

} // namespace woff2

#endif  // WOFF2_NORMALIZE_H_

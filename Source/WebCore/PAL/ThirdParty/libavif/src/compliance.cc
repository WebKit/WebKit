// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstddef>
#include <cstdint>
#include <limits>

#include "avif/internal.h"

// From ../ext/ComplianceWarden/src/utils/
#include "box_reader_impl.h"
#include "spec.h"

bool checkComplianceStd(Box const & file, SpecDesc const * spec);

SpecDesc const * specFind(const char * name);
std::vector<SpecDesc const *> & g_allSpecs();
extern const SpecDesc * const globalSpecAvif;
extern const SpecDesc * const globalSpecAv1Hdr10plus;
extern const SpecDesc * const globalSpecHeif;
extern const SpecDesc * const globalSpecIsobmff;
extern const SpecDesc * const globalSpecMiaf;

extern "C" avifResult avifIsCompliant(const uint8_t * data, size_t size)
{
    // See compliance_warden.sh.
    if (g_allSpecs().empty()) {
        registerSpec(globalSpecAvif);
        registerSpec(globalSpecAv1Hdr10plus);
        registerSpec(globalSpecHeif);
        registerSpec(globalSpecIsobmff);
        registerSpec(globalSpecMiaf);
    }

    // Inspired from ext/ComplianceWarden/src/app/cw.cpp
    BoxReader topReader;
    for (char sym : { 'f', 'i', 'l', 'e', '.', 'a', 'v', 'i', 'f' }) {
        // Setting made-up file name (letter by letter).
        topReader.myBox.syms.push_back({ "filename", static_cast<int64_t>(sym), 8 });
    }
    AVIF_CHECKERR(size <= std::numeric_limits<int>::max(), AVIF_RESULT_INVALID_ARGUMENT);
    topReader.br = { const_cast<uint8_t *>(data), static_cast<int>(size) };
    topReader.myBox.original = const_cast<uint8_t *>(data);
    topReader.myBox.position = 0;
    topReader.myBox.size = size;
    topReader.myBox.fourcc = FOURCC("root");
    topReader.specs = { specFind("avif") };
    AVIF_CHECKERR(topReader.specs[0] != nullptr, AVIF_RESULT_UNKNOWN_ERROR);
    auto parseFunc = getParseFunction(topReader.myBox.fourcc);
    parseFunc(&topReader);
    // gpac/ComplianceWarden will print the formatted result page to stdout, warnings and errors inclusive.
    AVIF_CHECKERR(!checkComplianceStd(topReader.myBox, topReader.specs[0]), AVIF_RESULT_BMFF_PARSE_FAILED);
    return AVIF_RESULT_OK;
}

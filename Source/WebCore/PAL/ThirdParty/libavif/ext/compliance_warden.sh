#!/bin/bash
#
# Local clone of ComplianceWarden (part of the gpac organization) for tests.
# Used when AVIF_ENABLE_COMPLIANCE_WARDEN is ON.

set -e

git clone https://github.com/gpac/ComplianceWarden.git
cd ComplianceWarden && git checkout e26973641f747cf3282004dcfb0747881207e748 && cd ..
# The provided Makefile only builds bin/cw.exe and objects.
# We are interested in the library, so the files are directly used instead of building with make -j.
ComplianceWarden/scripts/version.sh > ComplianceWarden/src/cw_version.cpp

# registerSpec() does not seem to be called in the static 'registered' local variables,
# for example in avif.cpp. Use the following hack to access the static SpecDescs.
# Feel free to replace by a prettier solution.
printf "extern const SpecDesc *const globalSpecAvif = &specAvif;\n" >> ComplianceWarden/src/specs/avif/avif.cpp
printf "extern const SpecDesc *const globalSpecAv1Hdr10plus = &specAv1Hdr10plus;\n" >> ComplianceWarden/src/specs/av1_hdr10plus/av1_hdr10plus.cpp
printf "extern const SpecDesc *const globalSpecHeif = &specHeif;\n" >> ComplianceWarden/src/specs/heif/heif.cpp
printf "extern const SpecDesc *const globalSpecIsobmff = &specIsobmff;\n" >> ComplianceWarden/src/specs/isobmff/isobmff.cpp
printf "extern const SpecDesc *const globalSpecMiaf = &specMiaf;\n" >> ComplianceWarden/src/specs/miaf/miaf.cpp

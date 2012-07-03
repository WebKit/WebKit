# -------------------------------------------------------------------
# This file is used by build-webkit to compute the various feature
# defines, which are then cached in .qmake.cache.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

# Will compute features based on command line arguments, config tests,
# dependency availability, and defaults.
load(features)

# Compute delta
CONFIG -= $$BASE_CONFIG
DEFINES -= $$BASE_DEFINES

message(CONFIG: $$CONFIG)
message(DEFINES: $$DEFINES)
error("Done computing defaults")

# SAR guard stripping — demo

**This directory is a TEST fixture only.** It doesn't build and isn't part of any target. It
exists to demonstrate, with real diffs, the `RC_REMOVE_SEC_ACCEL_*` guard shapes that
`WebkitSecurityMergebackIntegrator` now strips automatically on merge-back to `main`
(rdar://186094248, [automerge PR 2884](https://stashweb.sd.apple.com/projects/SAFARI/repos/automerge/pull-requests/2884)).

Each sibling PR in this fork adds one case file showing the guard **as it would have landed on
`main` before that change** — i.e. what a merge-back carried before the stripper existed. The PR
description shows what the stripper turns it into today.

# ANGLE Wrangling

As an ANGLE Sheriff. Your job is to:

 1. Keep the [ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/angle.try/builders) in good
    working order.
 1. Monitor the
    [Chromium GPU FYI Waterfall](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console)
    and respond to ANGLE bugs.
 1. Control and monitor the [ANGLE auto-rollers](#task-3_the-auto_rollers).
 1. Keep the [ANGLE Standalone Testers](README.md) in good working order.

If you're not an ANGLE team member, you can contact us on the public ANGLE project
[Google group](https://groups.google.com/forum/#!forum/angleproject).

## Task 1: The Try Waterfall

Your first job is to keep the
[ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/angle.try/builders) healthy.  Some
failures are expected on this waterfall. Developers might be testing expectedly buggy code. But
persistent flakiness and failures should be reported and reverted.

When encoutering an unexpected failure in your CLs please
[file an ANGLE bug](http://anglebug.com/new) and cc the current ANGLE wrangler. If the failure is
unrelated to ANGLE [file a Chromium bug](http://crbug.com/new) and mark the bug as
`Hotlist-PixelWrangler`. Refer to
[build.chromium.org](https://ci.chromium.org/p/chromium/g/main/console) to find the current ANGLE
wrangler and GPU Pixel Wrangler.

**NOTE: When all builds seem to be purple or otherwise broken:**

This could be a major infrastructure outage. File a high-priority bug using
[g.co/bugatrooper](http://g.co/bugatrooper).

## Task 2: Respond to Bugs

ANGLE bugs sometimes make it past the commit queue testing and into the master branch. This can be
because of flaky tests or because the failures are specific to system configurations for which we
lack full pre-commit testing support.

The [Chromium GPU FYI Waterfall](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console)
waterfall includes a number of these one-off specialized configurations.  Monitor this console for
persistent breakage that could be related to ANGLE.  Also follow the `Internals>GPU>ANGLE` component
on the Chromium issue tracker to be alerted to reports of breakage on the GPU.FYI waterfall.
Googlers can use [sheriff-o-matic](https://sheriff-o-matic.appspot.com/chromium.gpu.fyi) to monitor
the health of the GPU.FYI waterfall.

Note that the GPU Pixel Wrangler is responsible for the *Chromium* bugs.  Please file issues with
the tag `Hotlist-PixelWrangler` for bugs that aren't caused by ANGLE regressions.

*IMPORTANT*: Info to include in bug reports:

 * Links to all first failing builds (eg first windows failure, first mac failure, etc).
 * Related regression ranges. See below on how to determine the ANGLE regression range.
 * Relevant error messages.
 * Set components: `Internals>GPU` and/or `Internals>GPU>ANGLE`.
 * cc relevant sheriffs or blame suspects.
 * Set the `Hotlist-PixelWrangler` label.

### How to determine the ANGLE regression range on the GPU.FYI bots:

 1. Open the first failing and last passing builds.
 1. For test failures: record `parent_got_angle_revision` in both builds.
 1. For compile failures record `got_angle_revision`.
 1. Use this URL:
    `https://chromium.googlesource.com/angle/angle.git/+log/<last passing revision>..<first failing revision>`

## Task 3: The Auto-Rollers

The [ANGLE auto-roller](https://autoroll.skia.org/r/angle-chromium-autoroll) automatically updates
Chrome with the latest ANGLE changes.

 1. **Roller health**: You will be cc'ed on all rolls. Please check failed rolls to verify there is no blocking
    breakage.
 1. **Chrome Branching**: You are responsible for pausing the roller 24h before branch days, and resuming afterwards.
    See the [Chrome Release Schedule](https://chromiumdash.appspot.com/schedule).

We also use additional auto-rollers to roll third party libraries into ANGLE once per day:

 * [SPIRV-Headers into ANGLE](https://autoroll.skia.org/r/spirv-headers-angle-autoroll),
 * [SPIRV-Tools into ANGLE](https://autoroll.skia.org/r/spirv-tools-angle-autoroll) and
 * [glslang into ANGLE](https://autoroll.skia.org/r/glslang-angle-autoroll)

Please ensure these rollers are also healthy and unblocked.

## Task 4: ANGLE Standalone Testing

See more detailed instructions on by following [this link](README.md).

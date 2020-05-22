# ANGLE Wrangling

As an ANGLE Sheriff. Your job is to:

 1. Keep the [ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/tryserver.chromium.angle/builders) in good
    working order.
 1. Monitor the
    [Chromium GPU FYI Waterfall](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console)
    and respond to ANGLE bugs.
 1. Control and monitor the [ANGLE auto-rollers](#task-3_the-auto_rollers).
 1. Keep the [ANGLE Standalone Testers](README.md) in good working order.
 1. Keep the [SwANGLE Try Waterfall](https://luci-milo.appspot.com/p/chromium/g/tryserver.chromium.swangle/builders) in good
    working order.

If you're not an ANGLE team member, you can contact us on the public ANGLE project
[Google group](https://groups.google.com/forum/#!forum/angleproject).

**Note**: It's highly recommend that all wranglers install the [Chromium Flake Linker][Flaker]
extension for inspecting bot builds. It'll save you a lot of time.

[Flaker]: https://chrome.google.com/webstore/detail/flake-linker/boamnmbgmfnobomddmenbaicodgglkhc

## Task 1: Monitor ANGLE CI and Try Testers

Your first job is to keep the
[ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/tryserver.chromium.angle/builders) healthy.  Some
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

 * [SPIRV-Tools into ANGLE](https://autoroll.skia.org/r/spirv-tools-angle-autoroll)
 * [glslang into ANGLE](https://autoroll.skia.org/r/glslang-angle-autoroll)
 * [SwiftShader into ANGLE](https://autoroll.skia.org/r/swiftshader-angle-autoroll)
 * [Vulkan-Tools into ANGLE](https://autoroll.skia.org/r/vulkan-tools-angle-autoroll)
 * [Vulkan-Loader into ANGLE](https://autoroll.skia.org/r/vulkan-loader-angle-autoroll)
 * [Vulkan-Headers into ANGLE](https://autoroll.skia.org/r/vulkan-headers-angle-autoroll)
 * [Vulkan-ValidationLayers into ANGLE](https://autoroll.skia.org/r/vulkan-validation-layers-angle-autoroll)

Please ensure these rollers are also healthy and unblocked. You can trigger manual rolls using the
dashboards to land high-priority changes.

**NOTE: When Vulkan-Headers roll is broken:**

The Vulkan-Tools, Vulkan-Loader, and Vulkan-ValidationLayers repos all depend on the Vulkan-Headers
repo. When Vulkan-Headers updates, all of those repos have a roll process managed by LunarG to
update them for the new Vulkan-Headers. This usually takes 2-3 business days after the
Vulkan-Headers update. If Vulkan-Headers roll fails, pause the roller with a note that it should be
re-enabled when the dependent repos have been updated for the latest Vulkan-Headers changes. This
will require a manual roll if the two repos have to be rolled in unison. To perform a manual roll,
create a CL that updates the DEPS file with the new SHA1 value from the desired checkout of each
dependent repo. Once the manual roll lands, re-enable the auto-rollers for the relevant repos.

The autoroller configurations live in the [skia/buildbot repository](https://skia.googlesource.com/buildbot/)
in the [autoroll/config](https://skia.googlesource.com/buildbot/+/master/autoroll/config) folder.

## Task 4: ANGLE Standalone Testing

See more detailed instructions on by following [this link](README.md).

## Task 5: Monitor SwANGLE CI and Try Testers

Most important task here is to keep healthy the 2 SwANGLE bots on ANGLE CQ,
[linux-swangle-try-tot-angle-x64](https://luci-milo.appspot.com/p/chromium/builders/try/linux-swangle-try-tot-angle-x64)
and
[win-swangle-try-tot-angle-x86](https://luci-milo.appspot.com/p/chromium/builders/try/win-swangle-try-tot-angle-x86).
As well as the 2 SwANGLE bots used for ANGLE rolls on Chromium CQ,
[linux-swangle-try-x64](https://luci-milo.appspot.com/p/chromium/builders/try/linux-swangle-try-x64)
and
[win-swangle-try-x86](https://luci-milo.appspot.com/p/chromium/builders/try/win-swangle-try-x86).

Same instructions as for [Task 1](#task-1_monitor-angle-ci-and-try-testers) apply here.
Some failures on these bots may be due to SwiftShader changes, however.
The possible ways to handle these failures are:
1. If possible, suppress the failing tests in ANGLE, opening a bug to investigate these later.
1. If it is clear that an ANGLE CL caused a regression,
   consider whether reverting it or suppressing the failures is a better course of action.
1. If a SwiftShader CL is suspected, and the breakage is too severe to be suppressed,
   (a lot of tests fail in multiple suites),
   it is possible to revert the responsible SwiftShader roll into Chromium
   and open a SwiftShader [bug](http://go/swiftshaderbugs). SwiftShader rolls into Chromium
   should fail afterwards, but if the bad roll manages to reland,
   the [autoroller](https://autoroll.skia.org/r/swiftshader-chromium-autoroll) needs to be stopped.

A lower priority task here is to keep healthy all the SwANGLE
[CI](https://luci-milo.appspot.com/p/chromium/g/chromium.swangle/builders) and
[Try](https://luci-milo.appspot.com/p/chromium/g/tryserver.chromium.swangle/builders) bots.

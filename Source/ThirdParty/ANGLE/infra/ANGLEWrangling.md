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
 1. Monitor and respond to ANGLE's [Perf alerts](https://groups.google.com/u/0/a/chromium.org/g/angle-perf-alerts)

If you're not an ANGLE team member, you can contact us on the public ANGLE project
[Google group](https://groups.google.com/forum/#!forum/angleproject).

**Note**: Please review and if needed update the [wrangler schedule].

**Note**: It's highly recommend that all wranglers install the [Chromium Flake Linker][Flaker]
extension for inspecting bot builds. It'll save you a lot of time.

**Note**: If you need to suppress test failures (e.g. to keep an auto-roller unblocked), see
[Handling Test Failures](../doc/TestingAndProcesses.md).

[Flaker]: https://chrome.google.com/webstore/detail/flake-linker/boamnmbgmfnobomddmenbaicodgglkhc

[wrangler schedule]: https://rotations.corp.google.com/rotation/5080504293392384

## Task 1: Monitor ANGLE CI and Try Testers

Your first job is to keep the
[ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/tryserver.chromium.angle/builders)
healthy.  These are also known as the CQ (commit queue) bots.  Some
failures are expected on this waterfall. Developers might be testing expectedly buggy code. However,
persistent flakiness and failures should be reported and appropriate CLs reverted.

For ANGLE issues that require CLs, please [file an ANGLE bug](http://anglebug.com/new) and set
the Label `Hotlist-Wrangler` on the bug.
[Click here to see those bugs.][WranglerBugs]

[WranglerBugs]:https://bugs.chromium.org/p/angleproject/issues/list?q=Hotlist%3DWrangler&can=2

When encountering an unexpected failure in a CL that looks ANGLE related, please
[file an ANGLE bug](http://anglebug.com/new) and cc the current ANGLE wrangler. If the failure is
unrelated to ANGLE [file a Chromium bug](http://crbug.com/new) and set the Label
`Hotlist-PixelWrangler` on the bug (see [Filing Chromium Bug Reports](#filing-chromium-bug-reports) below).
Refer to [build.chromium.org](https://ci.chromium.org/p/chromium/g/main/console) to find the current ANGLE
wrangler and GPU Pixel Wrangler.

**NOTE: When all builds seem to be purple or otherwise broken:**

This could be a major infrastructure outage. File a high-priority bug using
[g.co/bugatrooper](http://g.co/bugatrooper).

## Task 2: Monitor Chromium FYI Testers and Respond to Bugs

ANGLE bugs sometimes make it past the commit queue testing and into the main branch. This can be
because of flaky tests or because the failures are specific to system configurations for which we
lack full pre-commit testing support.

The [Chromium GPU FYI Waterfall][ChromiumFYI]
waterfall includes a number of these one-off specialized configurations.  Monitor this console for
persistent breakage that could be related to ANGLE.  Also follow [Chromium bugs in the `Internals>GPU>ANGLE` component][ChromiumANGLEBugs]
to be alerted to reports of breakage on the GPU.FYI waterfall.

[ChromiumFYI]:https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console
[ChromiumANGLEBugs]:https://bugs.chromium.org/p/chromium/issues/list?q=component%3AInternals%3EGPU%3EANGLE&can=2

Googlers can use [sheriff-o-matic](https://sheriff-o-matic.appspot.com/chromium.gpu.fyi) to monitor
the health of the GPU.FYI waterfall.

### Filing Chromium Bug Reports

The GPU Pixel Wrangler is responsible for the *Chromium* bugs.  Please file issues with
the Label `Hotlist-PixelWrangler` for bugs that aren't caused by ANGLE regressions.

*IMPORTANT* info to include in Chromium bug reports:

 * Links to all first failing builds (e.g. first windows failure, first mac failure, etc).
 * Related regression ranges. See below on how to determine the ANGLE regression range.
 * Relevant error messages.
 * Set the **Components** to one or more value, such as (start typing "Internals" and you'll see choices):
   * `Internals>GPU` for general GPU bugs
   * `Internals>GPU>Testing` for failures that look infrastructure-related
   * `Internals>GPU>ANGLE` for ANGLE-related Chromium bugs
   * `Internals>Skia` for Skia-specific bugs
 * Cc relevant sheriffs or blame suspects, as well as yourself or the current ANGLE Wrangler.
 * Set the `Hotlist-PixelWrangler` Label.

### How to determine the ANGLE regression range on the GPU.FYI bots:

 1. Open the first failing and last passing builds.
 1. For test failures: record `parent_got_angle_revision` in both builds.
 1. For compile failures record `got_angle_revision`.
 1. Use this URL:
    `https://chromium.googlesource.com/angle/angle.git/+log/<last passing revision>..<first failing revision>`

## <a name="the-auto-rollers"></a>Task 3: The Auto-Rollers

The [ANGLE auto-roller](https://autoroll.skia.org/r/angle-chromium-autoroll) automatically updates
Chrome with the latest ANGLE changes.

 1. **Roller health**: You will be cc'ed on all rolls. Please check failed rolls to verify there is no blocking
    breakage.
 1. **Chrome Branching**: You are responsible for pausing the roller 24h before branch days, and resuming afterwards.
    See the [Chrome Release Schedule](https://chromiumdash.appspot.com/schedule).

We also use additional auto-rollers to roll third party libraries, and Chromium, into ANGLE once per day:

 * [SwiftShader into ANGLE](https://autoroll.skia.org/r/swiftshader-angle-autoroll)
 * [vulkan-deps into ANGLE](https://autoroll.skia.org/r/vulkan-deps-angle-autoroll)
 * [VK-GL-CTS into ANGLE](https://autoroll.skia.org/r/vk-gl-cts-angle-autoroll?tab=status)
 * [Chromium into ANGLE](https://autoroll.skia.org/r/chromium-angle-autoroll)

Please ensure these rollers are also healthy and unblocked. You can trigger manual rolls using the
dashboards to land high-priority changes, for example Chromium-side test expectation updates or
suppressions. When a roll fails, stop the roller, determine if the root cause is a problem with
ANGLE or with the upstream repo, and file an issue with an appropriate next step.

The autoroller configurations live in the [skia/buildbot repository](https://skia.googlesource.com/buildbot/)
in the [autoroll/config](https://skia.googlesource.com/buildbot/+/main/autoroll/config) folder.

**NOTE: vulkan-deps consists of several related Vulkan dependencies:**

vulkan-deps houses Vulkan-Tools, Vulkan-Loader, Vulkan-ValidationLayers, Vulkan-Headers and other
related repos. If the roll fails, you will have to determine the correct upstream repo and file
an issue upstream. For more info on vulkan-deps see the
[README](https://chromium.googlesource.com/vulkan-deps/+/refs/heads/main/README.md).

Occasionally, a vulkan-deps AutoRoll CL will get an error in the `presubmit` bot.  For example,
see: https://chromium-review.googlesource.com/c/angle/angle/+/3198390 where the
`export_targets.py` script had trouble with the `loader_windows.h` file.  The `export_targets.py`
script sometimes has difficulty with headers.  If you cannot see an obvious problem, create a CL
that adds the header to `IGNORED_INCLUDES` in `export_targets.py`.

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

## Task 6: Monitor and respond to ANGLE's perf alerts

Any large regressions should be triaged with a new ANGLE bug linked to any suspected CLs that may
have caused performance to regress. If it's a known/expected regression, the bug can be closed as
such. The tests are very flaky right now, so a WontFix resolution is often appropriate.

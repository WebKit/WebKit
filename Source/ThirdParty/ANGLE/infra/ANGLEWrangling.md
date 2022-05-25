# ANGLE Wrangling

As an ANGLE Sheriff. Your job is to:

 1. Keep the [ANGLE Try Waterfall](https://ci.chromium.org/p/chromium/g/tryserver.chromium.angle/builders) in good
    working order.
 1. Control and monitor the [ANGLE auto-rollers](#the-auto-rollers).
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

Your first job is to keep the ANGLE builders green and unblocked.

There are two consoles for ANGLE continuous integration builders:

 1. Standalone ANGLE: https://ci.chromium.org/p/angle/g/ci/console
 1. Chromium + ANGLE integration: https://ci.chromium.org/p/chromium/g/chromium.angle/console

We expect these waterfalls to be perfectly green at all times. As of writing
several builders are red or flaky. Oart of your job as wrangler is to
eliminate all sources of breaks and flakiness. We can fix flakiness by
suppressing tests that are not worth fixing, reverting problematic CLs, or
finding other solutions.

When you encouter red builds or flakiness, please [file an ANGLE bug]
(http://anglebug.com/new) and set the label: `Hotlist-Wrangler`.

[Click here to see a ANGLE wrangler bug hot list.][WranglerBugs]

In addition to the CI builders, we have a console for try jobs on the ANGLE CV (change verifier):

 * https://ci.chromium.org/ui/p/angle/g/try/builders

Some failures are expected on this waterfall as developers test WIP changes.
Please watch for persistent sources of flakiness and failure and take action
as appropriate by filing bugs, reverting CLs, or taking other action.

[WranglerBugs]:https://bugs.chromium.org/p/angleproject/issues/list?q=Hotlist%3DWrangler&can=2

If you find a point of failure that is unrelated to ANGLE, please [file a
Chromium bug](http://crbug.com/new). Set the bug label
`Hotlist-PixelWrangler`. Ensure you cc the current ANGLE and Chrome GPU
wranglers, which you can find by consulting
[build.chromium.org](https://ci.chromium.org/p/chromium/g/main/console).
For more information see [Filing Chromium Bug Reports](#filing-chromium-bug-reports) below.

Also follow [Chromium bugs in the `Internals>GPU>ANGLE` component][ChromiumANGLEBugs]
to be alerted to reports of ANGLE-related breakage in Chrome.

[ChromiumANGLEBugs]:https://bugs.chromium.org/p/chromium/issues/list?q=component%3AInternals%3EGPU%3EANGLE&can=2

**NOTE: When all builds seem to be purple or otherwise broken:**

This could be a major infrastructure outage. File a high-priority bug using
[g.co/bugatrooper](http://g.co/bugatrooper).

### Filing Chromium Bug Reports

The GPU Pixel Wrangler is responsible for *Chromium* bugs. Please file
Chromium issues with the Label `Hotlist-PixelWrangler` for bugs outside of
the ANGLE project.

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

### How to determine the ANGLE regression range on Chromium bots:

 1. Open the first failing and last passing builds.
 1. For test failures: record `parent_got_angle_revision` in both builds.
 1. For compile failures record `got_angle_revision`.
 1. Create a regression link with this URL template:
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

### SwANGLE builders

The ANGLE into Chromium roller has two SwiftShader + ANGLE (SwANGLE) builders:
[linux-swangle-try-x64](https://luci-milo.appspot.com/p/chromium/builders/try/linux-swangle-try-x64)
and
[win-swangle-try-x86](https://luci-milo.appspot.com/p/chromium/builders/try/win-swangle-try-x86).
However, failures on these bots may be due to SwiftShader changes.

To handle failures on these bots:
1. If possible, suppress the failing tests in ANGLE, opening a bug to investigate later.
1. If you supsect an ANGLE CL caused a regression,
   consider whether reverting it or suppressing the failures is a better course of action.
1. If you suspect a SwiftShader CL, and the breakage is too severe to suppress,
   (a lot of tests fail in multiple suites),
   consider reverting the responsible SwiftShader roll into Chromium
   and open a SwiftShader [bug](http://go/swiftshaderbugs). SwiftShader rolls into Chromium
   should fail afterwards, but if the bad roll manages to reland,
   stop the [autoroller](https://autoroll.skia.org/r/swiftshader-chromium-autoroll) as well.

## Task 4: ANGLE Standalone Testing

See more detailed instructions on by following [this link](README.md).

## Task 5: Monitor and respond to ANGLE's perf alerts

Any large regressions should be triaged with a new ANGLE bug linked to any suspected CLs that may
have caused performance to regress. If it's a known/expected regression, the bug can be closed as
such. The tests are very flaky right now, so a WontFix resolution is often appropriate.

# ANGLE Testing Infrastructure

ANGLE runs hundreds of thousands of tests on every change before it lands in
the tree. We scale our pre-commit and post-commit testing to many machines
using [Chromium Swarming][Swarming]. Our testing setup heavily leverages
existing work in Chromium. We also run standalone testing
that does not depend on a Chromium checkout.

## Pre-Commit Testing

We have pre-commit testing on [Chromium][ANGLEChromiumTry] and [ANGLE][StandaloneTry] try waterfalls.

We currently run pre-commit tests on:

 * Windows 64-bit Intel and NVIDIA GPUs and SwiftShader on VMs
 * Windows 32-bit on SwiftShader on VMs
 * Linux 64-bit Intel and NVIDIA GPUs and SwiftShader on VMs
 * Mac NVIDIA, Intel and AMD GPUs
 * Pixel 2, Pixel 4 and Pixel 6
 * Fuchsia compile-only
 * iOS simulator on demand

Looking at an example build shows how tests are split up between machines. See for example:

[`https://ci.chromium.org/ui/p/angle/builders/ci/mac-test/8117/overview`](https://ci.chromium.org/ui/p/angle/builders/ci/mac-test/8117/overview)

This build ran 67 test steps across 3 GPU families. In some cases (e.g.
`angle_deqp_gles3_metal_tests`) the test is split up between multiple machines to
run faster (in this case 2 different machines at once). This build took 10
minutes to complete 50 minutes of real automated testing.

For more details on running and working with our test sets see the docs in [Contributing Code][Contrib].

## Post-Commit Testing

Similarly to pre-commit testing, there are also [Chromium][ANGLEChromiumCI] and [ANGLE][StandaloneCI] CI (Continuous Integration) waterfalls.
These run on the same configurations as pre-commit, plus additional configurations for which we only have limited HW, e.g. Samsung S22 phones.
They are useful for detecting flaky failures and for regression blamelists in case some failure does manage to slip through pre-commit testing.

## Auto-Rollers

Some ANGLE dependencies are rolled in via automatically created CLs by these auto-rollers:
 * [SwiftShader](https://autoroll.skia.org/r/swiftshader-angle-autoroll)
 * [vulkan-deps](https://autoroll.skia.org/r/vulkan-deps-angle-autoroll)
 * [VK-GL-CTS](https://autoroll.skia.org/r/vk-gl-cts-angle-autoroll)
 * [Chromium build dependencies](https://autoroll.skia.org/r/chromium-angle-autoroll)

Similarly, Chromium's copy of ANGLE is updated by the [ANGLE into Chromium auto-roller](https://autoroll.skia.org/r/angle-chromium-autoroll).
And there also exists a [SwiftShader into Chromium auto-roller](https://autoroll.skia.org/r/swiftshader-chromium-autoroll).

[Swarming]: https://chromium-swarm.appspot.com/
[Contrib]: ../doc/ContributingCode.md#Testing
[ANGLEChromiumCI]: https://ci.chromium.org/p/chromium/g/chromium.angle/console
[ANGLEChromiumTry]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.angle/builders
[StandaloneCI]: https://ci.chromium.org/p/angle/g/ci/console
[StandaloneTry]: https://ci.chromium.org/ui/p/angle/g/try/builders



# ANGLE Standalone Testing Infrastructure

In addition to the ANGLE try bots using Chrome, and the GPU.FYI bots, ANGLE
has standalone testing on the Chrome infrastructure. Currently these tests are
compile-only. This page is for maintaining the configurations that don't use
Chromium. Also see the main instructions for [ANGLE Wrangling](ANGLEWrangling.md).

It's the ANGLE team's responsibility for maintaining this testing
infrastructure. The bot configurations live in four different repos and six
branches.

## Info Consoles

Continuous builders for every ANGLE revision are found on the CI console:

[https://ci.chromium.org/p/angle/g/ci/console](https://ci.chromium.org/p/angle/g/ci/console)

Try jobs from pre-commit builds are found on the builders console:

[https://ci.chromium.org/p/angle/g/try/builders](https://ci.chromium.org/p/angle/g/try/builders)

## How to add a new build configuration

 1. [`bugs.chromium.org/p/chromium/issues/entry?template=Build+Infrastructure`](http://bugs.chromium.org/p/chromium/issues/entry?template=Build+Infrastructure):

    * If adding a Mac bot, request new slaves by filing an infra issue.

 1. [`chrome-internal.googlesource.com/infradata/config`](http://chrome-internal.googlesource.com/infradata/config):

    * Update **`configs/chromium-swarm/starlark/bots/angle.star`** with either Mac slaves requested in the previous step or increase the amount of Windows or Linux GCEs.

 1. [`chromium.googlesource.com/chromium/tools/build`](https://chromium.googlesource.com/chromium/tools/build):

    * Update **`scripts/slave/recipes/angle.py`** with new the config.
    * The recipe code requires 100% code coverage through mock bots, so add mock bot config to GenTests.
    * Maybe run `./scripts/slave/recipes.py test train` to update checked-in golden files. This might no longer be necessary.

 1. [`chromium.googlesource.com/angle/angle`](http://chromium.googlesource.com/angle/angle):

    * Update **`infra/config/global/cr-buildbucket.cfg`** to add the new builder (to ci and try), and set the new config option.
    * Update **`infra/config/global/luci-milo.cfg`** to make the builders show up on the ci and try waterfalls.
    * Update **`infra/config/global/luci-scheduler.cfg`** to make the builders trigger on new commits or try jobs respectively.
    * Update **`infra/config/global/commit-queue.cfg`** to add the builder to the default CQ jobs (if desired).

## Other Configuration

There are other places where configuration for ANGLE infra lives. These are files that we shouldn't need to modify very often:

 1. [`chrome-internal.googlesource.com/infradata/config`](http://chrome-internal.googlesource.com/infradata/config):

    * **`configs/luci-token-server/service_accounts.cfg`** (service account names)
    * **`configs/chromium-swarm/pools.cfg`** (swarming pools)

 1. [`chromium.googlesource.com/chromium/tools/depot_tools`](http://chromium.googlesource.com/chromium/tools/depot_tools):

    * **`recipes/recipe_modules/gclient/config.py`** (gclient config)

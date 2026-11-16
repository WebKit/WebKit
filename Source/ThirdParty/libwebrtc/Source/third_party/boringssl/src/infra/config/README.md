# BoringSSL Builder Configs

This directory contains the builder configs for BoringSSL's CI and CQ.
To modify it, edit `main.star` and then rerun `./main.star` to regenerate the
generated files. Also remember to run `lucicfg fmt main.star` to format the
file.

Avoid too much implicit logic in `main.star`. The file only exists to generate
a modest number of builders, so we can optimize for reading the file and
minimizing surprises over removing all redundancy.

Changes to this file are picked up asynchronously, after the change lands. In
particular, running the CQ on a change will *not* test the changes you are
making. This unfortunately leads to testing things live.

BoringSSL is small, so breaking CI/CQ is not an emergency. Still, when making
risky changes, prefer to trial them. Some strategies:

* Simulate changes in the source tree first, e.g. by modifying `CMakeLists.txt`
  or `util/bot/DEPS`, and running the CQ on a temporary CL. Builder configs work
  differently, so this is not ideal, but it can be pretty close.

* Add new builders as disabled, CQ-only builders first. Run them manually to
  test them, and only enable on CI and CQ after the pass.

Builder configs reference recipes, which live in
[a separate repository](https://chromium.googlesource.com/chromium/tools/build/+/main/recipes/recipes).
Like changes to this directory, recipe changes are not atomic and are picked up
some time after the change lands.

This means we often need to make multi-sided commits across the recipes, this
directory, and BoringSSL itself. To reduce the need for this, the recipe tries
to defer as much to builder configs as possible with generic properties.

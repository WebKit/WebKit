# The WebKit Linux Developer SDK

## Tips and Tricks

This is aimed to replace the JHBuild setup, as announced in [webkit-dev](https://lists.webkit.org/pipermail/webkit-dev/2020-March/031147.html). Most
of the time developers won't need to manually build the SDK, they will download
already-built versions of it.

Rebuilding the SDK is needed when you have to update WebKit test dependencies or
add new dependencies to the SDK.

To build the SDK, run:

```bash
Tools/Scripts/webkit-flatpak-sdk --build
```

This can take hours and use gigabytes of your hard-drive. To build a single recipe, use something
like:

```bash
Tools/Scripts/bst-wrapper build sdk/gtk.bst
```

To enter the Buildstream SDK shell and debug stuff when hacking on a specific
dependency:

```bash
Tools/Scripts/bst-wrapper shell sdk/gtk.bst
```

To test your changes in WebKit:

```bash
Tools/Scripts/webkit-flatpak -u --repo=$PWD/Tools/buildstream/repo
Tools/Scripts/run-minibrowser --gtk --flatpak-repo=$PWD/Tools/buildstream/repo <url>
Tools/Scripts/run-webkit-tests --gtk --repo=$PWD/Tools/buildstream/repo
...
```

bst-wrapper has the same options as bst itself, documented on the [Buildstream website](https://docs.buildstream.build/1.6/index.html).

Recipe version updates should follow this procedure:

- For recipes tracking git tags (with the `git_tag` source kind), you can manually update the branch
  or tag name referenced in the `track` value (see rr.bst for example). and then run `bst-wrapper
  track sdk/<recipe>.bst`. If the recipe tracks git master, no manual update is needed, just run the
  track command.
- For recipes tracking tarballs, update the `url` value and run `bst-wrapper track
  sdk/<recipe>.bst`.


Another nice feature of Buildstream is `workspace`, where you can make changes
to recipes:

```bash
$ Tools/Scripts/bst-wrapper workspace open --directory=~/Projects/openxr sdk/openxr.bst
# hack hack hack in ~/Projects/openxr/
$ Tools/Scripts/bst-wrapper build sdk/openxr.bst
# Once you are happy, format changes from your workspace with git:
$ cd ~/Projects/openxr
$ git commit -a ...
$ git format-patch -1
$ mv *.patch ~/WebKit/Tools/buildstream/patches/
# finally add patches as sources in openxr.bst
# and close the workspace
$ Tools/Scripts/bst-wrapper workspace close sdk/openxr.bst
$ rm -fr ~/Projects/openxr
```

Internally the bst-wrapper will install Buildstream and a few other python
dependencies in a local virtualenv managed by pipenv. This is all abstracted
though, in theory direct access to the pipenv environment shouldn't be needed.

## Maintenance

This section attempts to document how to do yearly major version upgrades of the SDK. The WebKit SDK
inherits from the FDO SDK, which has one year release cycle. They release a new major version around
end of August, beginning of September. When the time has come, here are the steps to follow to
synchronize our SDK with its parent.

### Preparation

1. Remove `Tools/buildstream/cache/`. The ostree repositories there should ideally be automatically
   re-created when the build is done for a new version, but this is not plumbed in the Makefile yet.
2. In `project.conf` change the `sdk-branch` to the new version.
3. Remove `~/.cache/buildstream`

### Updating FDO junction and recipes

1. Update the `freedesktop-sdk.bst` junction:
   - Update `track:` to track new release branch
   - Review the list of junction patches we maintain there. Usually most of these are no longer
     relevant for the new version.
   - Run `bst-wrapper source track freedesktop-sdk.bst` to update the junction reference. Tracking will
     fail if any of our patches fail to apply. This should give you hints that some manual update or
     removal of the patch is needed.
2. Build the SDK. This is the most time consuming task. Sometimes a recipe will fail to build and
   will need an update... `webkit-flatpak-sdk --build
   EXPORT_ARGS="--gpg-sign=9A0495AF96828F9D5E032C46A9A60744BCE3F878 --gpg-homedir=gpg" all`. A
   non-exhaustive list of issues that can happen:
   - A recipe that wasn't provided by upstream SDK and instead was part of our downstream SDK, is
     now upstream. Downstream recipe is now redundant and should be removed
   - Compiler updates in upstream SDK often lead to build failures in our SDK. Usually bumping the
     version of recipes helps with that...
   - Recipe version upgrades also often lead to sub-rabbit holes. You've been warned.

Once you have a successful build (🥳), the time has come to try a local-only uprade. Gather all
these changes in a new branch, to be submitted after testing.

### Local smoke testing of the new SDK

Important note. Don't include the `flatpakutils.py` update in the branch that was prepared earlier.
This change is critical and it is of utmost importance to land this change carefully **AFTER** the
remote flatpak repo has been updated to the new version (which is documented in next section).

1. Update `SDK_BRANCH` in `flatpakutils.py`.
2. Run `webkit-flatpak -u --repo=$PWD/Tools/buildstream/repo` which will detect a version
   upgrade, remove the UserFlatpak.Local and install the new SDK, from scratch.
3. Update your WPE and GTK builds, don't forget to set the
   `WEBKIT_FLATPAK_USER_DIR=$HOME/WebKit/WebKitBuild/UserFlatpak.Local` environment variable so that
   the local SDK is used. This goes for the test runs below as well.
4. Run the layout tests: `run-webkit-tests --gtk`
5. Run the API tests: `webkit-flatpak -c $PWD/Tools/Scripts/run-gtk-tests` and `run-wpe-tests`
6. Run the webkitpy tests: `webkit-flatpak -c $PWD/Tools/Scripts/test-webkitpy`
7. Run the webkitperl tests: `webkit-flatpak -c $PWD/Tools/Scripts/test-webkitperl`
8. Run the test262 tests: `webkit-flatpak -c $PWD/Tools/Scripts/test262-runner --gtk --release`
9. Run the JS bindings tests: `webkit-flatpak -c $PWD/Tools/Scripts/run-bindings-tests`
10. Run the JSC tests: `webkit-flatpak -c $PWD/Tools/Scripts/run-javascriptcore-tests --gtk`

Once you're confident the SDK is usable and doesn't break hundreds of tests, submit the first PR
that updates the recipes for review. Once that PR has landed you can proceed to next section.

### Deploy the OSTree updates to the HTTP server

You need SSH access to `software.igalia.com` for this.

```bash
webkit-flatpak-sdk --build push-repo
```

Once the remote repository is updated, the new version is available and can even be installed with a
manual `flatpak install` call. The next step is to teach the WebKit tooling to use this new
version. You can proceed to next section.

### Making the update available to all users

Submit the `SDK_BRANCH` update of `flatpakutils.py` to review in a new PR. Additional changes might
be needed, like layout tests rebaselines for instance.

Once this PR has landed, all bots and developers pulling from WebKit's git main branch and running
`webkit-flatpak -u` will get the new version.

### Update the Epiphany Canary flatpak manifest

The Epiphany Canary flavour depends on the WebKit SDK Platform runtime. So the [Canary
manifest](https://gitlab.gnome.org/GNOME/epiphany/-/blob/master/org.gnome.Epiphany.Canary.json.in)
needs to be updated, the `runtime-version` should point to the new SDK version.

Mission accomplished. 🤝

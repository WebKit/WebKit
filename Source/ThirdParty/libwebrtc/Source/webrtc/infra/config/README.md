# infra/config folder

This folder contains WebRTC project-wide configurations for Chrome infra
services, mainly the CI system ([console][]).

`*.cfg` files are the actual configuration that [LUCI][luci-config] looks at.
They are in *protocol buffer text format*. For example,
[cr-buildbucket.cfg](cr-buildbucket.cfg) defines builders.

However, they are all automatically generated from the [Starlark][] script
[config.star](config.star) that defines a unified config using **[lucicfg][]**.
The main body of the config is at the bottom of the file, following all the
helper definitions.

`lucicfg` should be available as part of depot_tools. After editing
[config.star](config.star) you should run `lucicfg generate config.star` to
re-generate `*.cfg` files. Check the diffs in generated files to confirm that
your change worked as expected. Both the code change and the generated changes
need to be committed together.

## Uploading changes

It is recommended to have a separate checkout for this branch, so switching
to/from it does not populate/delete all files in the master branch.

Initial setup:

```bash
git clone https://webrtc.googlesource.com/src/
```

Now you can create a new branch to make changes:

```bash
git new-branch add-new-builder
# edit/generate files
git commit -a
git cl upload
```

Changes can be reviewed on Gerrit and submitted with commit queue as usual.

### Activating the changes

Any changes to this directory go live soon after landing, without any additional
steps. You can see the status or force a refresh of the config at
[luci-config][].

[console]: https://ci.chromium.org/p/webrtc/g/ci/console
[luci-config]: https://luci-config.appspot.com/#/projects/webrtc
[starlark]: https://github.com/google/starlark-go
[lucicfg]: https://chromium.googlesource.com/infra/luci/luci-go/+/master/lucicfg/doc/

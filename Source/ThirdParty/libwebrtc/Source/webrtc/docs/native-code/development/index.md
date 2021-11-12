# WebRTC development

The currently supported platforms are Windows, Mac OS X, Linux, Android and
iOS. See the [Android][webrtc-android-development] and [iOS][webrtc-ios-development]
pages for build instructions and example applications specific to these mobile platforms.


## Before You Start

First, be sure to install the [prerequisite software][webrtc-prerequisite-sw].

[webrtc-prerequisite-sw]: https://webrtc.googlesource.com/src/+/main/docs/native-code/development/prerequisite-sw/index.md


## Getting the Code

For desktop development:

1. Create a working directory, enter it, and run `fetch webrtc`:

```
$ mkdir webrtc-checkout
$ cd webrtc-checkout
$ fetch --nohooks webrtc
$ gclient sync
```

NOTICE: During your first sync, you'll have to accept the license agreement of the Google Play Services SDK.

The checkout size is large due the use of the Chromium build toolchain and many dependencies. Estimated size:

* Linux: 6.4 GB.
* Linux (with Android): 16 GB (of which ~8 GB is Android SDK+NDK images).
* Mac (with iOS support): 5.6GB

2. Optionally you can specify how new branches should be tracked:

```
$ git config branch.autosetupmerge always
$ git config branch.autosetuprebase always
```

3. Alternatively, you can create new local branches like this (recommended):

```
$ cd src
$ git checkout main
$ git new-branch your-branch-name
```

See the [Android][webrtc-android-development] and [iOS][webrtc-ios-development] pages for separate instructions.

**NOTICE:** if you get `Remote: Daily bandwidth rate limit exceeded for <ip>`,
make sure you're logged in. The quota is much larger for logged in users.

## Updating the Code

Update your current branch with:

```
$ git checkout main
$ git pull origin main
$ gclient sync
$ git checkout my-branch
$ git merge main
```

## Building

[Ninja][ninja] is the default build system for all platforms.

See the [Android][webrtc-android-development] and [iOS][webrtc-ios-development] pages for build
instructions specific to those platforms.

## Generating Ninja project files

[Ninja][ninja] project files are generated using [GN][gn]. They're put in a
directory of your choice, like `out/Debug` or `out/Release`, but you can
use any directory for keeping multiple configurations handy.

To generate project files using the defaults (Debug build), run (standing in
the src/ directory of your checkout):

```
$ gn gen out/Default
```

To generate ninja project files for a Release build instead:

```
$ gn gen out/Default --args='is_debug=false'
```

To clean all build artifacts in a directory but leave the current GN
configuration untouched (stored in the args.gn file), do:

```
$ gn clean out/Default
```

To build the fuzzers residing in the [test/fuzzers][fuzzers] directory, use
```
$ gn gen out/fuzzers --args='use_libfuzzer=true optimize_for_fuzzing=true'
```
Depending on the fuzzer additional arguments like `is_asan`, `is_msan` or `is_ubsan_security` might be required.

See the [GN][gn-doc] documentation for all available options. There are also more
platform specific tips on the [Android][webrtc-android-development] and
[iOS][webrtc-ios-development] instructions.

## Compiling

When you have Ninja project files generated (see previous section), compile
(standing in `src/`) using:

For [Ninja][ninja] project files generated in `out/Default`:

```
$ ninja -C out/Default
```

To build everything in the generated folder (`out/Default`):

```
$ ninja all -C out/Default
```

See [Ninja build rules][ninja-build-rules] to read more about difference between `ninja` and `ninja all`.


## Using Another Build System

Other build systems are **not supported** (and may fail), such as Visual
Studio on Windows or Xcode on OSX. GN supports a hybrid approach of using
[Ninja][ninja] for building, but Visual Studio/Xcode for editing and driving
compilation.

To generate IDE project files, pass the `--ide` flag to the [GN][gn] command.
See the [GN reference][gn-doc] for more details on the supported IDEs.


## Working with Release Branches

To see available release branches, run:

```
$ git branch -r
```

To create a local branch tracking a remote release branch (in this example,
the branch corresponding to Chrome M80):

```
$ git checkout -b my_branch refs/remotes/branch-heads/3987
$ gclient sync
```

**NOTICE**: depot_tools are not tracked with your checkout, so it's possible gclient
sync will break on sufficiently old branches. In that case, you can try using
an older depot_tools:

```
which gclient
$ # cd to depot_tools dir
$ # edit update_depot_tools; add an exit command at the top of the file
$ git log  # find a hash close to the date when the branch happened
$ git checkout <hash>
$ cd ~/dev/webrtc/src
$ gclient sync
$ # When done, go back to depot_tools, git reset --hard, run gclient again and
$ # verify the current branch becomes REMOTE:origin/main
```

The above is untested and unsupported, but it might help.

Commit log for the branch: [https://webrtc.googlesource.com/src/+log/branch-heads/3987][m80-log]
To browse it: [https://webrtc.googlesource.com/src/+/branch-heads/3987][m80]

For more details, read Chromium's [Working with Branches][chromium-work-branches] and
[Working with Release Branches][chromium-work-release-branches] pages.
To find the branch corresponding to a Chrome release check the
[Chromium Dashboard][https://chromiumdash.appspot.com/branches].


## Contributing Patches

Please see [Contributing Fixes][webrtc-contributing] for information on how to run
`git cl upload`, getting your patch reviewed, and getting it submitted. You can also
find info on how to run trybots and applying for try rights.

## Chromium Committers

Many WebRTC committers are also Chromium committers. To make sure to use the
right account for pushing commits to WebRTC, use the `user.email` Git config
setting. The recommended way is to have the chromium.org account set globally
as described at the [depot tools setup page][depot-tools] and then set `user.email`
locally for the WebRTC repos using (change to your webrtc.org address):

```
$ cd /path/to/webrtc/src
$ git config user.email yourname@webrtc.org
```

## Example Applications

WebRTC contains several example applications, which can be found under
`src/webrtc/examples`. Higher level applications are listed first.


### Peerconnection

Peerconnection consist of two applications using the WebRTC Native APIs:

* A server application, with target name `peerconnection_server`
* A client application, with target name `peerconnection_client` (not currently supported on Mac/Android)

The client application has simple voice and video capabilities. The server
enables client applications to initiate a call between clients by managing
signaling messages generated by the clients.


#### Setting up P2P calls between peerconnection_clients

Start `peerconnection_server`. You should see the following message indicating
that it is running:

```
Server listening on port 8888
```

Start any number of `peerconnection_clients` and connect them to the server.
The client UI consists of a few parts:

**Connecting to a server:** When the application is started you must specify
which machine (by IP address) the server application is running on. Once that
is done you can press **Connect** or the return button.

**Select a peer:** Once successfully connected to a server, you can connect to
a peer by double-clicking or select+press return on a peer's name.

**Video chat:** When a peer has been successfully connected to, a video chat
will be displayed in full window.

**Ending chat session:** Press **Esc**. You will now be back to selecting a
peer.

**Ending connection:** Press **Esc** and you will now be able to select which
server to connect to.


#### Testing peerconnection_server

Start an instance of `peerconnection_server` application.

Open `src/webrtc/examples/peerconnection/server/server_test.html` in your
browser. Click **Connect**. Observe that the `peerconnection_server` announces
your connection. Open one more tab using the same page. Connect it too (with a
different name). It is now possible to exchange messages between the connected
peers.

### STUN Server

Target name `stunserver`. Implements the STUN protocol for Session Traversal
Utilities for NAT as documented in [RFC 5389][rfc-5389].


### TURN Server

Target name `turnserver`. Used for unit tests.


[ninja]: https://ninja-build.org/
[ninja-build-rules]: https://gn.googlesource.com/gn/+/master/docs/reference.md#the-all-and-default-rules
[gn]: https://gn.googlesource.com/gn/+/master/README.md
[gn-doc]: https://gn.googlesource.com/gn/+/master/docs/reference.md#IDE-options
[webrtc-android-development]: https://webrtc.googlesource.com/src/+/main/docs/native-code/android/index.md
[webrtc-ios-development]: https://webrtc.googlesource.com/src/+/main/docs/native-code/ios/index.md
[chromium-work-branches]: https://www.chromium.org/developers/how-tos/get-the-code/working-with-branches
[chromium-work-release-branches]: https://www.chromium.org/developers/how-tos/get-the-code/working-with-release-branches
[webrtc-contributing]: https://webrtc.org/support/contributing/
[depot-tools]: http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
[rfc-5389]: https://tools.ietf.org/html/rfc5389
[rfc-5766]: https://tools.ietf.org/html/rfc5766
[m80-log]: https://webrtc.googlesource.com/src/+log/branch-heads/3987
[m80]: https://webrtc.googlesource.com/src/+/branch-heads/3987
[fuzzers]: https://webrtc.googlesource.com/src/+/main/test/fuzzers/

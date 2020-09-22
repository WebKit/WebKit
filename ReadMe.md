# WebKit

WebKit is a cross-platform web browser engine. On iOS and macOS, it powers Safari, Mail, iBooks, and many other applications.

## Feature Status

Visit [WebKit Feature Status](https://webkit.org/status/) page to see which Web API has been implemented, in development, or under consideration.

## Trying the Latest

On macOS, [download Safari Technology Preview](https://webkit.org/downloads/) to test the latest version of WebKit. On Linux, download [Epiphany Technology Preview](https://webkitgtk.org/epiphany-tech-preview). On Windows, you'll have to build it yourself.

## Reporting Bugs

1. [Search WebKit Bugzilla](https://bugs.webkit.org/query.cgi?format=specific&product=WebKit) to see if there is an existing report for the bug you've encountered.
2. [Create a Bugzilla account](https://bugs.webkit.org/createaccount.cgi) to to report bugs (and to comment on them) if you haven't done so already.
3. File a bug in accordance with [our guidelines](https://webkit.org/bug-report-guidelines/).

Once your bug is filed, you will receive email when it is updated at each stage in the [bug life cycle](https://webkit.org/bug-life-cycle). After the bug is considered fixed, you may be asked to download the [latest nightly](https://webkit.org/nightly) and confirm that the fix works for you.

## Getting the Code

On Windows, follow the [instructions on our website](https://webkit.org/webkit-on-windows/).

### Cloning the Git SVN Repository

Run the following command to clone WebKit's Git SVN repository:

```
git clone git://git.webkit.org/WebKit.git WebKit
```

or

```
git clone https://git.webkit.org/git/WebKit.git WebKit
```

If you want to be able to commit changes to the repository, or just want to check out branches that aren’t contained in WebKit.git, you will need track WebKit's Subversion repository. You can run the following command to configure this and other options of the new Git clone for WebKit development.

```
Tools/Scripts/webkit-patch setup-git-clone
```

For information about this, and other aspects of using Git with WebKit, read [the wiki page](https://trac.webkit.org/wiki/UsingGitWithWebKit).

### Checking out the Subversion Repository

If you don't want to use Git, run the following command to check out WebKit's Subversion repository:

```
svn checkout https://svn.webkit.org/repository/webkit/trunk WebKit
```

## Building WebKit

### Building macOS Port

Install Xcode and its command line tools if you haven't done so already:

1. **Install Xcode** Get Xcode from https://developer.apple.com/downloads. To build WebKit for OS X, Xcode 5.1.1 or later is required. To build WebKit for iOS Simulator, Xcode 7 or later is required.
2. **Install the Xcode Command Line Tools** In Terminal, run the command: `xcode-select --install`

Run the following command to build a debug build with debugging symbols and assertions:

```
Tools/Scripts/build-webkit --debug
```

For performance testing, and other purposes, use `--release` instead.

### Using Xcode

You can open `WebKit.xcworkspace` to build and debug WebKit within Xcode.

If you don't use a custom build location in Xcode preferences, you have to update the workspace settings to use `WebKitBuild` directory.  In menu bar, choose File > Workspace Settings, then click the Advanced button, select "Custom", "Relative to Workspace", and enter `WebKitBuild` for both Products and Intermediates.

### Embedded Builds

iOS, tvOS and watchOS are all considered embedded builds. The first time after you install a new Xcode, you will need to run:

```
sudo Tools/Scripts/configure-xcode-for-ios-development
```

Without this step, you will see the error message: "`target specifies product type ‘com.apple.product-type.tool’, but there’s no such product type for the ‘iphonesimulator’ platform.`" when building target `JSCLLIntOffsetsExtractor` of project `JavaScriptCore`.

Run the following command to build a debug build with debugging symbols and assertions for embededded simulators:

```
Tools/Scripts/build-webkit --debug --<platform>-simulator
```

or embedded devices:
```
Tools/Scripts/build-webkit --debug --<platform>-device
```

where `platform` is `ios`, `tvos` or `watchos`.

### Building the GTK+ Port

For production builds:

```
cmake -DPORT=GTK -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
ninja
sudo ninja install
```

For development builds:

```
Tools/gtk/install-dependencies
Tools/Scripts/update-webkitgtk-libs
Tools/Scripts/build-webkit --gtk --debug
```

For more information on building WebKitGTK+, see the [wiki page](https://trac.webkit.org/wiki/BuildingGtk).

### Building the WPE Port

For production builds:

```
cmake -DPORT=WPE -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
ninja
sudo ninja install
```

For development builds:

```
Tools/wpe/install-dependencies
Tools/Scripts/update-webkitwpe-libs
Tools/Scripts/build-webkit --wpe --debug
```

### Building Windows Port

For building WebKit on Windows, see the [wiki page](https://webkit.org/webkit-on-windows/).

## Running WebKit

### With Safari and Other macOS Applications

Run the following command to launch Safari with your local build of WebKit:

```
Tools/Scripts/run-safari --debug
```

The `run-safari` script sets the `DYLD_FRAMEWORK_PATH` environment variable to point to your build products, and then launches `/Applications/Safari.app`. `DYLD_FRAMEWORK_PATH` tells the system loader to prefer your build products over the frameworks installed in `/System/Library/Frameworks`.

To run other applications with your local build of WebKit, run the following command:

```
Tools/Scripts/run-webkit-app <application-path>
```

### iOS Simulator

Run the following command to launch iOS simulator with your local build of WebKit:

```
run-safari --debug --ios-simulator
```

In both cases, if you have built release builds instead, use `--release` instead of `--debug`.

### Linux Ports

If you have a development build, you can use the run-minibrowser script, e.g.:

```
run-minibrowser --debug --wpe
```

Pass one of `--gtk`, `--jsc-only`, or `--wpe` to indicate the port to use.

## Contribute

Congratulations! You’re up and running. Now you can begin coding in WebKit and contribute your fixes and new features to the project. For details on submitting your code to the project, read [Contributing Code](https://webkit.org/contributing-code/).


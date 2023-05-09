# Yocto-based cross-build support on WebKit build scripts

This documents how the cross-build support on the WebKit build scripts works for Yocto-based targets.

## The script 'cross-toolchain-helper'

The script ```cross-toolchain-helper``` is the one responsible for building the cross-toolchain and the images
for any supported target. The other WebKit scripts that support cross-building (like build-webkit) end calling
this ```cross-toolchain-helper``` script to integrate with it.

This script also allows to start a development shell for the target.

For more info run: ```${WebKitDirectory}/Tools/Scripts/cross-toolchain-helper --help``` and also read the
documenation and examples below

### The targets.conf file

This script can build a cross-toolchain (a Yocto SDK) and a image for a specific board.
The boards supported are defined on the file ```${WebKitDirectory}/Tools/yocto/targets.conf```

This configuration file defined the boards (targets) supported in a windows-ini-file-like
(python-configparser) syntax.

The script ```cross-toolchain-helper``` will parse this file and it will do the following:

1. Clone the Yocto repositories defined in ```repo_manifest_path``` (with the ```repo``` tool)
2. Apply the patch (if defined) ```patch_file_path``` on top of this repositories
3. Build the Yocto SDK for the target ```image_basename``` and unpack the SDK built.
4. Build the Yocto image ```image_basename```

For more info see the file ```${WebKitDirectory}/Tools/yocto/targets.conf```


### Integration with WebKit tooling script

The support for cross-building is integrated with the following WebKit scripts:

	- Tools/Scripts/update-webkitwpe-libs
	- Tools/Scripts/update-webkitgtk-libs
	- Tools/Scripts/build-webkit
	- Tools/CISupport/built-product-archive

You can pass the flag ```--cross-target=${target_name}``` to any of this
scripts and they will build or pack the archive for the specified target.

If you are inside a ```cross-dev-shell``` then you don't need to pass this flag
because this scripts will also understand the environment variable ```WEBKIT_TARGET_CROSS```
that is automatically exported when you enter inside a cross-dev-shell

### Using the cross-dev-shell

If you want to develop or debug for the target board, then you can enter
into a ```cross-dev-shell``` with the following command:

```
Tools/Scripts/cross-toolchain-helper --cross-target=target-board --cross-dev-shell
```

Inside this shell, if you call the script ```build-webkit``` then it will automatically
detect that you are cross-building for the target (without needing to pass the ```--cross-target```
flag).

Anything you build (other software) should also be built for the target board.

You have there also a debugger enabled to debug binaries for the target board.

Inside this shell, you use ```$CC``` and ```$CXX``` as compilers for the target.
A cross-aware debugger is also enabled, you can invoke it with: ```$GDB```


```
user@workstation(WKCrossDevShell:rpi3-32bits-mesa): ~/WebKit $ env | grep -P '^(CC|CXX|LD|AR|GDB|.*WEBKIT.*)='
BUILD_WEBKIT_ARGS=--no-fatal-warnings --cmakeargs="-DENABLE_WPE_QT_API=OFF -DENABLE_THUNDER=OFF -DENABLE_DOCUMENTATION=OFF -DENABLE_INTROSPECTION=OFF -DWPE_COG_PLATFORMS=drm,headless,wayland"
GDB=arm-poky-linux-gnueabi-gdb
CXX=arm-poky-linux-gnueabi-g++ -mthumb -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/home/igalia/webkit/WebKitBuild/CrossToolChains/rpi3-32bits-mesa/build/toolchain/sysroots/cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
LD=arm-poky-linux-gnueabi-ld  --sysroot=/home/igalia/webkit/WebKitBuild/CrossToolChains/rpi3-32bits-mesa/build/toolchain/sysroots/cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
AR=arm-poky-linux-gnueabi-ar
CC=arm-poky-linux-gnueabi-gcc -mthumb -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/home/igalia/webkit/WebKitBuild/CrossToolChains/rpi3-32bits-mesa/build/toolchain/sysroots/cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
WEBKIT_CROSS_TARGET=rpi3-32bits-mesa
```


### Using the bitbake-dev-shell

If you want to debug or tune something on the bitbake build system used
to build to cross-toolchain or the image you can enter into a bitbake
shell for the target with this command:

```
Tools/Scripts/cross-toolchain-helper --cross-target=target-board --bitbake-dev-shell
```

There you can run ```bitbake``` commands for the generated Yocto project.

### Speeding the build with an external Yocto cache

Rebuilding the whole toolchain and image from scratch is slow, and if you
are developing with this you will end having to rebuild the toolchains and
images from scratch usually.

A trick to speed up the whole rebuild process is to store the yocto sstate
and download caches outside of the WebKitBuild directory, so they can be
reused between different builds.

To enable that, add this lines to your ```~/.bashrc``` file

```
# Export shared directories for yocto caches
export DL_DIR="${HOME}/.cache/yocto/downloads"
export SSTATE_DIR="${HOME}/.cache/yocto/sstate"
export BB_ENV_PASSTHROUGH_ADDITIONS="${BB_ENV_PASSTHROUGH_ADDITIONS} DL_DIR SSTATE_DIR"
```

### Disabling automatic wipe on change

The script ```cross-toolchain-helper``` will automatically wipe and rebuild the
cross-toolchain and images for the target board if it detects anything has changed
on the target source definitions.

To avoid this you can pass the flag ```--no-wipe-on-change``` to ```cross-toolchain-helper```
or you can set the environment variable ```WEBKIT_CROSS_WIPE_ON_CHANGE=0```

However it is not recommended that you do this, instead try to enable an external Yocto cache
to have very fast rebuilds (see above).


### Disabling export default environment

The script ```cross-toolchain-helper``` will automatically export several environment variables
defined in file ```${WebKitDirectory}/Tools/yocto/targets.conf``` for the given target.
This environment variables are usually used to change the default build parameters for the
script ```build-webkit```

If you want to disable this you can pass the flag ```--no-export-default-environment``` to ```cross-toolchain-helper```
or you can set the environment variable ```WEBKIT_CROSS_EXPORT_ENV=0```


## Practical examples on how to use this

This is an example on how to build and run WPE for development on the RPi:

### Build the base image, flash the board and clone the WebKit repo

1. Build the image that will be flashed on the board:

```
user@workstation $ Tools/Scripts/cross-toolchain-helper --cross-target=rpi3-32bits-mesa --build-image
```

2. At the end of the build it will print the path to the image, if you missed it
   run the command again and it will print the path again (doesn't rebuild if not needed).
   Example:

```
user@workstation $ Tools/Scripts/cross-toolchain-helper --cross-target=rpi3-32bits-mesa --build-image
cross-toolchain-helper INFO: Image already built at: /home/igalia/webkit/WebKitBuild/CrossToolChains/rpi3-32bits-mesa/build/image/core-image-webkit-dev-ci-tools-tests.wic.xz
```

3. Flash a SDCard with this image

  * 3.1. Use bmaptool for a faster copy

```
# deb distro: sudo apt install bmap-tools
# rpm distro: sudo dnf install bmap-tools
user@workstation $ bmaptool copy /path/to/core-image-webkit-dev-ci-tools-tests.wic.xz /dev/mmcblk0
```

  * 3.2. Or alternatively use dd

```
user@workstation $ cat /path/to/core-image-webkit-dev-ci-tools-tests.wic.xz | xz -dc | dd of=/dev/mmcblk0 status=progress bs=4k
```

  * 3.3. Or alternatively use the script for deploying

```
user@workstation $ DEVICESDCARD=/dev/mmcblk0 Tools/Scripts/cross-toolchain-helper --cross-target=rpi3-32bits-mesa --deploy-image-with-script Tools/yocto/deploy_image_scripts/wic-to-sdcard.sh
```

4. Grow the rootfs partition to use all the available space on the SDcard

```
# install growfs tool if not available
# rpm distro: sudo dnf install cloud-utils-growpart
# deb distro: sudo apt install cloud-guest-utils
user@workstation $ sudo growpart /dev/mmcblk0 2
user@workstation $ sudo resize2fs /dev/mmcblk0p2
```

5. Connect the RPi via Ethernet (wire) to your network, insert the sdcard and boot it

```
# Connect Ethernet wire to RPi
# Insert SDCard, boot RPi and obtain the IP address asigned via DHCP
# NOTE: password for root is empty (no auth required)
user@workstation $ ssh root@192.168.X.Y
root@raspberrypi3:~# git clone https://github.com/WebKit/WebKit.git --depth 1
````

### Build WPE for the target board

1. You can use the build-webkit script passing the flag --cross-target=name

```
user@workstation $ Tools/Scripts/build-webkit --wpe --release --cross-target=rpi3-32bits-mesa
```

2. The build will be stored in a per-target directory: ```WebKitBuild/WPE/Release_rpi3-32bits-mesa```

This allows to build simultaneously for different targets (and for your host system)
from the same checkout.

3. Generate a release.zip for copying to the target board.

```
Tools/CISupport/built-product-archive --platform=wpe --release --cross-target=rpi3-32bits-mesa archive
```

4. Copy release.zip to the board

```
scp WebKitBuild/release.zip root@192.168.X.Y:
```


### Unpack the built product on the board and run the browser or run tests

1. Unpack the release.zip previously copied as follows:

```
# cd to the webkit checkout in the board and copy release.zip inside the build directory
user@workstation $ ssh root@192.168.X.Y

root@raspberrypi3:~# cd WebKit/
root@raspberrypi3:~/WebKit# mkdir WebKitBuild
root@raspberrypi3:~/WebKit# cp ~/release.zip WebKitBuild/

root@raspberrypi3:~/WebKit# Tools/CISupport/built-product-archive --platform=wpe --release extract
Extracting: /home/root/WebKit/WebKitBuild/Release
Archive:  /home/root/WebKit/WebKitBuild/release.zip
[....]
```

2. Now you can run the minibrowser (cog) or any type of tests.

```
# [Example 1]: Run Cog over the framebuffer with KMS/DRM
# first: ensure weston is stopped
root@raspberrypi3:~/WebKit# systemctl stop weston
# Run cog with platform plugin 'drm'
root@raspberrypi3:~/WebKit# Tools/Scripts/run-minibrowser --wpe -P drm

# [Example 2]: Run Cog inside Weston/Wayland
# first: start weston
root@raspberrypi3:~/WebKit# systemctl start weston
# define and export the required environment variables
root@raspberrypi3:~/WebKit# source <(strings /proc/$(pidof weston-desktop-shell)/environ | grep -P '(XDG_RUNTIME_DIR|WAYLAND_DISPLAY)')
root@raspberrypi3:~/WebKit# export XDG_RUNTIME_DIR
root@raspberrypi3:~/WebKit# export WAYLAND_DISPLAY
# Run cog with platform plugin 'wl'
root@raspberrypi3:~/WebKit# Tools/Scripts/run-minibrowser --wpe -P wl


# [Example 3]: Run layout tests
root@raspberrypi3:~/WebKit# Tools/Scripts/run-webkit-tests --debug-rwt-logging --no-build --wpe --release fast/css

# Note: the board may be not powerful enough or have not enough ram.
# If you see issues with tests crashing or timing out try to lower the
# number of parallel tests and raise the timeout value for the tests.
# [Example 4]: Run layout tests with one worker:
root@raspberrypi3:~/WebKit# export NUMBER_OF_PROCESSORS=1
root@raspberrypi3:~/WebKit# Tools/Scripts/run-webkit-tests --debug-rwt-logging --no-build --wpe --release fast/css
```

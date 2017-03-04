This directory contains an example Android client for https://appr.tc

Prerequisites:
- "Getting the code", "Compiling", and "Using the Bundled Android SDK/NDK"
  on http://www.webrtc.org/native-code/android

Example of building & using the app:

cd <path/to/webrtc>/src
ninja -C out/Default AppRTCMobile
adb install -r out/Default/apks/AppRTCMobile.apk

In desktop chrome, navigate to https://appr.tc and note the r=<NNN> room
this redirects to or navigate directly to https://appr.tc/r/<NNN> with
your own room number. Launch AppRTC on the device and add same <NNN> into the room name list.

You can also run application from a command line to connect to the first room in a list:
adb shell am start -n org.appspot.apprtc/.ConnectActivity -a android.intent.action.VIEW
This should result in the app launching on Android and connecting to the 3-dot-apprtc
page displayed in the desktop browser.
To run loopback test execute following command:
adb shell am start -n org.appspot.apprtc/.ConnectActivity -a android.intent.action.VIEW --ez "org.appspot.apprtc.LOOPBACK" true


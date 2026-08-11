# ANGLE Settings UI

## Introduction

ANGLE Settings UI is a UI shipped as part of the ANGLE apk. The ANGLE apk can ship with a list rules
of applications using different OpenGL ES driver. The UI provides two functionalities:

1) Enable to show a message when ANGLE is loaded into the launched application;
2) Allow to select driver choice in the UI for applications.

## The rule file

Currently the ANGLE apk supports two rules: ANGLE and native OpenGL ES driver.

The rule file is a file that contains a JSON string, the format is shown below:

```
{
    "rules":[
        {
            "description": "Applications in this list will use ANGLE",
            "choice": "angle",
            "apps": [
                {
                    "packageName": "com.android.example.a"
                },
                {
                    "packageName": "com.android.example.b"
                }
            ]
        },
        {
            "description": "Applications in this list will not use ANGLE",
            "choice": "native",
            "apps":[
                {
                    "packageName": "com.android.example.c"
                }
            ]
        }
    ]
}
```

The ANGLE JSON rules are parsed only when `ACTION_BOOT_COMPLETED` or `ACTION_MY_PACKAGE_REPLACED` is
received. After the JSON rules are parsed, the result will be merged into `Settings.Global` if no
conflicting user preference exists. The JSON parsing code is in `AngleRuleHelper`.

After parsing, the rules are synchronized to global settings variables. This is done in `Receiver`
and `GlobalSettings`.

The UI logic is mainly in `MainFragment`. `GlobalSettings` handles reading from and writing to
`Settings.Global`. When a user changes the driver choice of an application, the update is written
directly to `Settings.Global`.

`Settings.Global` is the source of truth (`angle_gl_driver_selection_pkgs` and
`angle_gl_driver_selection_values`). The code queries the driver choice from `GlobalSettings` which
reads from `Settings.Global`.

The settings global variables may also be changed via `adb` command by the users.
Note that every time a boot event happens, defaults from the ANGLE JSON rule file are applied ONLY if
there is no existing setting for that package, preserving user preferences.

## Developer options

The ANGLE Settings UI is registered as a dynamic setting entry in the development component via

```
<intent-filter>
    <action android:name="com.android.settings.action.IA_SETTINGS" />
</intent-filter>
<meta-data android:name="com.android.settings.category"
        android:value="com.android.settings.category.ia.development" />
```

The ANGLE Settings UI can be found in the Settings app named "ANGLE Preferences", or launched
manually via `adb`:

```bash
adb shell am start -n com.android.angle/com.android.angle.MainActivity
```

## How to build and install

You can build the specific module `ANGLE_settings` from the Android root directory:

```bash
m ANGLE_settings
```

This will generate an APK in the product output directory, typically:
`out/target/product/<device>/system/priv-app/ANGLE_settings/ANGLE_settings.apk`

To install/update it on a device:

```bash
adb install -r -g out/target/product/<device>/system/priv-app/ANGLE_settings/ANGLE_settings.apk
```

You can also build the entire ANGLE project with:

```bash
m ANGLE
```

## Using ADB to Manage Settings

You can view and modify the ANGLE settings directly using `adb`. Because the generic system UI
listens for changes to `Settings.Global`, any updates made via `adb` will naturally reflect in the
UI.

To **get** the current driver selection packages and values:

```bash
adb shell settings get global angle_gl_driver_selection_pkgs
adb shell settings get global angle_gl_driver_selection_values
```

To **set** the driver selection (e.g., enable ANGLE for `com.android.calculator2`):

```bash
adb shell settings put global angle_gl_driver_selection_pkgs "com.android.calculator2"
adb shell settings put global angle_gl_driver_selection_values "angle"
```

To **reset** settings to their default values, please use the "Reset to default values" button in
the Developer Options UI. This ensures all settings are cleared and default rules are correctly
re-applied.

Alternatively, you can manually delete the global settings and reboot the device to trigger the
initialization logic:

```bash
adb shell settings delete global angle_gl_driver_selection_pkgs
adb shell settings delete global angle_gl_driver_selection_values
adb reboot
```

### UI Synchronization

The ANGLE Settings app monitors `Settings.Global` for changes. If you modify these values via `adb`
while the Settings app is open, the UI list will automatically refresh to show the new state. This
ensures the UI is always in sync with the underlying system settings.

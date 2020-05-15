/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.angle.common;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.provider.Settings;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

class GlobalSettings
{

    private final String TAG = this.getClass().getSimpleName();

    private Context mContext;
    private List<PackageInfo> mInstalledPkgs         = new ArrayList<>();
    private List<String> mGlobalSettingsDriverPkgs   = new ArrayList<>();
    private List<String> mGlobalSettingsDriverValues = new ArrayList<>();

    GlobalSettings(Context context, List<PackageInfo> installedPkgs)
    {
        mContext = context;

        setInstalledPkgs(installedPkgs);
    }

    static void clearAllGlobalSettings(Context context)
    {
        // angle_gl_driver_all_angle
        updateAllUseAngle(context, false);
        // show_angle_in_use_dialog_box
        updateShowAngleInUseDialog(context, false);
        // angle_gl_driver_selection_pkgs, angle_gl_driver_selection_values
        ContentResolver contentResolver = context.getContentResolver();
        Settings.Global.putString(contentResolver,
                context.getString(R.string.global_settings_driver_selection_pkgs), "\"\"");
        Settings.Global.putString(contentResolver,
                context.getString(R.string.global_settings_driver_selection_values), "\"\"");

        // For completeness, we'll clear the angle_debug_package, but we don't allow setting
        // it via the APK (currently)
        Settings.Global.putString(contentResolver,
                context.getString(R.string.global_settings_angle_debug_package), "");

        // Skip angle_whitelist; not updatable via Developer Options
    }

    Boolean getAllUseAngle()
    {
        ContentResolver contentResolver = mContext.getContentResolver();
        try
        {
            int allUseAngle = Settings.Global.getInt(
                    contentResolver, mContext.getString(R.string.global_settings_driver_all_angle));
            return (allUseAngle == 1);
        }
        catch (Settings.SettingNotFoundException e)
        {
            return false;
        }
    }

    Boolean getShowAngleInUseDialogBox()
    {
        ContentResolver contentResolver = mContext.getContentResolver();
        try
        {
            int showAngleInUseDialogBox = Settings.Global.getInt(contentResolver,
                    mContext.getString(R.string.global_settings_show_angle_in_use_dialog_box));
            return (showAngleInUseDialogBox == 1);
        }
        catch (Settings.SettingNotFoundException e)
        {
            return false;
        }
    }

    static void updateAllUseAngle(Context context, Boolean allUseAngle)
    {
        ContentResolver contentResolver = context.getContentResolver();
        Settings.Global.putInt(contentResolver,
                context.getString(R.string.global_settings_driver_all_angle), allUseAngle ? 1 : 0);
    }

    static void updateShowAngleInUseDialog(Context context, Boolean showAngleInUseDialog)
    {
        ContentResolver contentResolver = context.getContentResolver();
        Settings.Global.putInt(contentResolver,
                context.getString(R.string.global_settings_show_angle_in_use_dialog_box),
                showAngleInUseDialog ? 1 : 0);
    }

    static void updateAngleWhitelist(Context context, String packageNames)
    {
        ContentResolver contentResolver = context.getContentResolver();
        Settings.Global.putString(contentResolver,
                context.getString(R.string.global_settings_angle_whitelist), packageNames);
    }

    void updatePkg(String pkgName, String driver)
    {
        int pkgIndex = getGlobalSettingsPkgIndex(pkgName);

        if (!isValidDiverValue(driver))
        {
            Log.e(TAG, "Attempting to update a PKG with an invalid driver: '" + driver + "'");
            return;
        }

        String defaultDriver = mContext.getString(R.string.default_driver);
        if (driver.equals(defaultDriver))
        {
            if (pkgIndex >= 0)
            {
                // We only store global settings for driver values other than the default
                mGlobalSettingsDriverPkgs.remove(pkgIndex);
                mGlobalSettingsDriverValues.remove(pkgIndex);
            }
        }
        else
        {
            if (pkgIndex >= 0)
            {
                mGlobalSettingsDriverValues.set(pkgIndex, driver);
            }
            else
            {
                mGlobalSettingsDriverPkgs.add(pkgName);
                mGlobalSettingsDriverValues.add(driver);
            }
        }

        writeGlobalSettings();
    }

    String getDriverForPkg(String pkgName)
    {
        int pkgIndex = getGlobalSettingsPkgIndex(pkgName);

        if (pkgIndex >= 0)
        {
            return mGlobalSettingsDriverValues.get(pkgIndex);
        }

        return null;
    }

    void setInstalledPkgs(List<PackageInfo> installedPkgs)
    {
        mInstalledPkgs = installedPkgs;

        updateGlobalSettings();
    }

    private void updateGlobalSettings()
    {
        readGlobalSettings();

        validateGlobalSettings();

        writeGlobalSettings();
    }

    private void readGlobalSettings()
    {
        mGlobalSettingsDriverPkgs = getGlobalSettingsString(
                mContext.getString(R.string.global_settings_driver_selection_pkgs));
        mGlobalSettingsDriverValues = getGlobalSettingsString(
                mContext.getString(R.string.global_settings_driver_selection_values));
    }

    private List<String> getGlobalSettingsString(String globalSetting)
    {
        List<String> valueList;
        ContentResolver contentResolver = mContext.getContentResolver();
        String settingsValue            = Settings.Global.getString(contentResolver, globalSetting);

        if (settingsValue != null)
        {
            valueList = new ArrayList<>(Arrays.asList(settingsValue.split(",")));
        }
        else
        {
            valueList = new ArrayList<>();
        }

        return valueList;
    }

    private void writeGlobalSettings()
    {
        String driverPkgs   = String.join(",", mGlobalSettingsDriverPkgs);
        String driverValues = String.join(",", mGlobalSettingsDriverValues);

        ContentResolver contentResolver = mContext.getContentResolver();
        Settings.Global.putString(contentResolver,
                mContext.getString(R.string.global_settings_driver_selection_pkgs), driverPkgs);
        Settings.Global.putString(contentResolver,
                mContext.getString(R.string.global_settings_driver_selection_values), driverValues);
    }

    private void validateGlobalSettings()
    {
        // Verify lengths
        if (mGlobalSettingsDriverPkgs.size() != mGlobalSettingsDriverValues.size())
        {
            // The lengths don't match, so clear the values out and rebuild later.
            mGlobalSettingsDriverPkgs.clear();
            mGlobalSettingsDriverValues.clear();
            return;
        }

        String defaultDriver = mContext.getString(R.string.default_driver);
        // Use a temp list, since we're potentially modifying the original list.
        List<String> globalSettingsDriverPkgs = new ArrayList<>(mGlobalSettingsDriverPkgs);
        for (String pkgName : globalSettingsDriverPkgs)
        {
            // Remove any uninstalled packages.
            if (!isPkgInstalled(pkgName))
            {
                removePkgFromGlobalSettings(pkgName);
            }
            // Remove any packages with invalid driver values
            else if (!isValidDiverValue(getDriverForPkg(pkgName)))
            {
                removePkgFromGlobalSettings(pkgName);
            }
            // Remove any packages with the 'default' driver selected
            else if (defaultDriver.equals(getDriverForPkg(pkgName)))
            {
                removePkgFromGlobalSettings(pkgName);
            }
        }
    }

    private void removePkgFromGlobalSettings(String pkgName)
    {
        int pkgIndex = getGlobalSettingsPkgIndex(pkgName);

        mGlobalSettingsDriverPkgs.remove(pkgIndex);
        mGlobalSettingsDriverValues.remove(pkgIndex);
    }

    private int getGlobalSettingsPkgIndex(String pkgName)
    {
        for (int pkgIndex = 0; pkgIndex < mGlobalSettingsDriverPkgs.size(); pkgIndex++)
        {
            if (mGlobalSettingsDriverPkgs.get(pkgIndex).equals(pkgName))
            {
                return pkgIndex;
            }
        }

        return -1;
    }

    private Boolean isPkgInstalled(String pkgName)
    {
        for (PackageInfo pkg : mInstalledPkgs)
        {
            if (pkg.packageName.equals(pkgName))
            {
                return true;
            }
        }

        return false;
    }

    private Boolean isValidDiverValue(String driverValue)
    {
        CharSequence[] drivers = mContext.getResources().getStringArray(R.array.driver_values);

        for (CharSequence driver : drivers)
        {
            if (driverValue.equals(driver.toString()))
            {
                return true;
            }
        }

        return false;
    }
}

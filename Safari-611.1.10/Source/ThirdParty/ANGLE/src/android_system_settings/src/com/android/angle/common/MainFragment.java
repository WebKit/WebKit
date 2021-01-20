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

import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragment;
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreference;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

public class MainFragment extends PreferenceFragment implements OnSharedPreferenceChangeListener
{

    private final String TAG = this.getClass().getSimpleName();

    private SharedPreferences mPrefs;
    private GlobalSettings mGlobalSettings;
    private Receiver mRefreshReceiver = new Receiver();
    private SwitchPreference mAllAngleSwitchPref;
    private SwitchPreference mShowAngleInUseDialogSwitchPref;
    private List<PackageInfo> mInstalledPkgs      = new ArrayList<>();
    private List<ListPreference> mDriverListPrefs = new ArrayList<>();

    SharedPreferences.OnSharedPreferenceChangeListener listener =
            new SharedPreferences.OnSharedPreferenceChangeListener() {
                public void onSharedPreferenceChanged(SharedPreferences prefs, String key)
                {
                    // Nothing to do yet
                }
            };

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        getInstalledPkgsList();

        mPrefs = PreferenceManager.getDefaultSharedPreferences(
                getActivity().getApplicationContext());
        validatePreferences();

        mGlobalSettings = new GlobalSettings(getContext(), mInstalledPkgs);
        mergeGlobalSettings();

        createAllUseAnglePreference();
        createShowAngleInUseDialogPreference();

        updatePreferencesFromGlobalSettings();
    }

    @Override
    public void onResume()
    {
        super.onResume();

        updatePreferencesFromGlobalSettings();

        getActivity().registerReceiver(mRefreshReceiver,
                new IntentFilter(
                        getContext().getString(R.string.intent_angle_for_android_toast_message)));
        getPreferenceScreen().getSharedPreferences().registerOnSharedPreferenceChangeListener(
                listener);
    }

    @Override
    public void onPause()
    {
        getActivity().unregisterReceiver(mRefreshReceiver);
        getPreferenceScreen().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(
                listener);

        super.onPause();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
    {
        addPreferencesFromResource(R.xml.main);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key)
    {
        Log.v(TAG, "Shared preference changed: key = '" + key + "'");
    }

    private void updatePreferencesFromGlobalSettings()
    {
        mAllAngleSwitchPref.setChecked(mGlobalSettings.getAllUseAngle());
        mShowAngleInUseDialogSwitchPref.setChecked(mGlobalSettings.getShowAngleInUseDialogBox());
        createInstalledAppsListPreference();
    }

    private void createAllUseAnglePreference()
    {
        String allUseAngleKey = getContext().getString(R.string.pref_key_all_angle);
        Boolean allUseAngle   = mPrefs.getBoolean(allUseAngleKey, false);
        mAllAngleSwitchPref   = (SwitchPreference) findPreference(allUseAngleKey);
        mAllAngleSwitchPref.setOnPreferenceClickListener(
                new Preference.OnPreferenceClickListener() {
                    @Override
                    public boolean onPreferenceClick(Preference preference)
                    {
                        Receiver.updateAllUseAngle(getContext());
                        return true;
                    }
                });
    }

    private void createShowAngleInUseDialogPreference()
    {
        String showAngleInUseDialogKey =
                getContext().getString(R.string.pref_key_angle_in_use_dialog);
        Boolean showAngleInUseDialogBox = mPrefs.getBoolean(showAngleInUseDialogKey, false);
        mShowAngleInUseDialogSwitchPref =
                (SwitchPreference) findPreference(showAngleInUseDialogKey);
        mShowAngleInUseDialogSwitchPref.setOnPreferenceClickListener(
                new Preference.OnPreferenceClickListener() {
                    @Override
                    public boolean onPreferenceClick(Preference preference)
                    {
                        Receiver.updateShowAngleInUseDialogBox(getContext());
                        return true;
                    }
                });
    }

    private void createInstalledAppsListPreference()
    {
        getInstalledPkgsList();
        mGlobalSettings.setInstalledPkgs(mInstalledPkgs);
        mergeGlobalSettings();

        String selectDriverCatKey =
                getContext().getString(R.string.pref_key_select_opengl_driver_category);
        PreferenceCategory installedPkgsCat =
                (PreferenceCategory) findPreference(selectDriverCatKey);
        installedPkgsCat.removeAll();
        mDriverListPrefs.clear();
        if (mInstalledPkgs.isEmpty())
        {
            ListPreference listPreference = new ListPreference(installedPkgsCat.getContext());
            initEmptyListPreference(listPreference);
            installedPkgsCat.addPreference(listPreference);
        }
        else
        {
            for (PackageInfo packageInfo : mInstalledPkgs)
            {
                ListPreference listPreference = new ListPreference(installedPkgsCat.getContext());
                initListPreference(packageInfo, listPreference);
                installedPkgsCat.addPreference(listPreference);
            }
        }
    }

    private void validatePreferences()
    {
        Map<String, ?> allPrefs = mPrefs.getAll();

        // Remove Preference values for any uninstalled PKGs
        for (String key : allPrefs.keySet())
        {
            // Remove any uninstalled PKGs
            PackageInfo packageInfo = getPackageInfoForPackageName(key);

            if (packageInfo != null)
            {
                removePkgPreference(key);
            }
        }
    }

    private void getInstalledPkgsList()
    {
        List<PackageInfo> pkgs = getActivity().getPackageManager().getInstalledPackages(0);

        mInstalledPkgs.clear();

        for (PackageInfo packageInfo : pkgs)
        {
            if (packageInfo.applicationInfo.uid == Process.SYSTEM_UID)
            {
                continue;
            }

            // Filter out apps that are system apps
            if ((packageInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0)
            {
                continue;
            }

            mInstalledPkgs.add(packageInfo);
        }

        Collections.sort(mInstalledPkgs, displayNameComparator);
    }

    private final Comparator<PackageInfo> displayNameComparator = new Comparator<PackageInfo>() {
        public final int compare(PackageInfo a, PackageInfo b)
        {
            return collator.compare(getAppName(a), getAppName(b));
        }

        private final Collator collator = Collator.getInstance();
    };

    private String getAppName(PackageInfo packageInfo)
    {
        return packageInfo.applicationInfo.loadLabel(getActivity().getPackageManager()).toString();
    }

    private void initEmptyListPreference(ListPreference listPreference)
    {
        String noAppsInstalledTitle = getContext().getString(R.string.no_apps_installed_title);
        listPreference.setTitle(noAppsInstalledTitle);

        String noAppsInstalledSummary = getContext().getString(R.string.no_apps_installed_summary);
        listPreference.setSummary(noAppsInstalledSummary);

        listPreference.setSelectable(false);

        mDriverListPrefs.add(listPreference);
    }

    private void initListPreference(PackageInfo packageInfo, ListPreference listPreference)
    {
        CharSequence[] drivers = getResources().getStringArray(R.array.driver_values);
        listPreference.setEntryValues(drivers);
        listPreference.setEntries(drivers);

        String defaultDriver = getContext().getString(R.string.default_driver);
        listPreference.setDefaultValue(defaultDriver);

        String dialogTitleKey = getContext().getString(R.string.select_opengl_driver_title);
        listPreference.setDialogTitle(dialogTitleKey);
        listPreference.setKey(packageInfo.packageName);

        listPreference.setTitle(getAppName(packageInfo));
        listPreference.setSummary(mPrefs.getString(packageInfo.packageName, defaultDriver));

        listPreference.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue)
            {
                ListPreference listPreference = (ListPreference) preference;

                listPreference.setSummary(newValue.toString());
                mGlobalSettings.updatePkg(preference.getKey(), newValue.toString());

                return true;
            }
        });

        mDriverListPrefs.add(listPreference);
    }

    private void removePkgPreference(String key)
    {
        SharedPreferences.Editor editor = mPrefs.edit();

        editor.remove(key);
        editor.apply();

        for (ListPreference listPreference : mDriverListPrefs)
        {
            if (listPreference.getKey().equals(key))
            {
                mDriverListPrefs.remove(listPreference);
            }
        }
    }

    private PackageInfo getPackageInfoForPackageName(String pkgName)
    {
        PackageInfo foundPackageInfo = null;

        for (PackageInfo packageInfo : mInstalledPkgs)
        {
            if (pkgName.equals(getAppName(packageInfo)))
            {
                foundPackageInfo = packageInfo;
                break;
            }
        }

        return foundPackageInfo;
    }

    private void mergeGlobalSettings()
    {
        SharedPreferences.Editor editor = mPrefs.edit();

        for (PackageInfo packageInfo : mInstalledPkgs)
        {
            String driver = mGlobalSettings.getDriverForPkg(packageInfo.packageName);

            if (driver != null)
            {
                editor.putString(packageInfo.packageName, driver);
            }
            else
            {
                // No Global.Setting driver value for this package, so must be 'default'
                String defaultDriver = getContext().getString(R.string.default_driver);
                editor.putString(packageInfo.packageName, defaultDriver);
            }
        }

        editor.apply();
    }
}

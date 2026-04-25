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

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.database.ContentObserver;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreference;

import com.android.angle.R;

import java.lang.reflect.Method;
import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class MainFragment extends PreferenceFragmentCompat
{

    private final String TAG = this.getClass().getSimpleName();
    private final boolean DEBUG = false;

    private boolean mIsAngleSystemDriver;
    private SwitchPreference mShowAngleInUseSwitchPref;
    private GlobalSettings mGlobalSettings;
    private ContentObserver mGlobalSettingsObserver;
    private Receiver mRefreshReceiver = new Receiver();
    private List<PackageInfo> mInstalledPackages = new ArrayList<>();

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        queryInstalledPackages();

        final String eglDriver = getSystemProperty("ro.hardware.egl", "");
        mIsAngleSystemDriver = eglDriver.equals("angle");
        if (DEBUG)
        {
            Log.d(TAG, "ro.hardware.egl = " + eglDriver);
        }

        final AngleRuleHelper angleRuleHelper = new AngleRuleHelper(getContext());
        mGlobalSettings = new GlobalSettings(getContext(),
                angleRuleHelper.getPackageNamesForAngle(),
                angleRuleHelper.getPackageNamesForNative());

        createShowAngleInUseSwitchPreference();
        createInstalledAppsListPreference();

        mGlobalSettingsObserver = new ContentObserver(new android.os.Handler())
        {
            @Override
            public void onChange(boolean selfChange)
            {
                mGlobalSettings.syncFromGlobalSettings();
                updateInstalledAppsListPreference();
            }
        };
    }

    @Override
    public void onResume()
    {
        super.onResume();

        getActivity().registerReceiver(mRefreshReceiver,
                new IntentFilter(
                        getContext().getString(R.string.intent_angle_for_android_toast_message)),
                Context.RECEIVER_EXPORTED);

        getContext().getContentResolver().registerContentObserver(
                android.provider.Settings.Global.getUriFor(
                        GlobalSettings.DRIVER_SELECTION_PACKAGES), false,
                mGlobalSettingsObserver);
        getContext().getContentResolver().registerContentObserver(
                android.provider.Settings.Global.getUriFor(GlobalSettings.DRIVER_SELECTION_VALUES),
                false,
                mGlobalSettingsObserver);

        updatePreferences();
    }

    @Override
    public void onPause()
    {
        getActivity().unregisterReceiver(mRefreshReceiver);
        getContext().getContentResolver().unregisterContentObserver(mGlobalSettingsObserver);

        super.onPause();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState)
    {
        super.onViewCreated(view, savedInstanceState);

        ViewCompat.setOnApplyWindowInsetsListener(view, (v, windowInsets) -> {
          Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
          MarginLayoutParams mlp = (MarginLayoutParams) v.getLayoutParams();
          mlp.topMargin = insets.top;
          v.setLayoutParams(mlp);
          return WindowInsetsCompat.CONSUMED;
        });
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
    {
        addPreferencesFromResource(R.xml.main);
    }

    private void updatePreferences()
    {
        mShowAngleInUseSwitchPref.setChecked(mGlobalSettings.getShowAngleInUse());
    }

    private void createShowAngleInUseSwitchPreference()
    {
        final String showAngleInUseKey =
                getContext().getString(R.string.pref_key_angle_in_use_dialog);
        mShowAngleInUseSwitchPref =
                (SwitchPreference) findPreference(showAngleInUseKey);

        mShowAngleInUseSwitchPref.setOnPreferenceChangeListener(
                new Preference.OnPreferenceChangeListener() {
                    @Override
                    public boolean onPreferenceChange(Preference preference, Object newValue)
                    {
                        if (DEBUG)
                        {
                            Log.v(TAG, "Show angle in use switch: " + newValue.toString());
                        }
                        GlobalSettings.updateShowAngleInUse(getContext(), (Boolean) newValue);
                        return true;
                    }
        });
    }

    private static String getSystemProperty(String key, String defaultValue) {
        try {
            Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            Method getMethod = systemProperties.getMethod("get", String.class, String.class);
            return (String) getMethod.invoke(null, key, defaultValue);
        } catch (Exception e) {
            e.printStackTrace();
            return defaultValue;
        }
    }

    private void createInstalledAppsListPreference()
    {
        final String selectDriverCategoryKey = getContext().getString(R.string.pref_key_select_opengl_driver_category);
        final PreferenceCategory preferenceCategory = (PreferenceCategory) findPreference(selectDriverCategoryKey);
        preferenceCategory.removeAll();

        // Find the "Reset to defaults" button and attach the click listener
        final Preference resetPreference = findPreference("reset_to_defaults");
        if (resetPreference != null) {
            resetPreference.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    new android.app.AlertDialog.Builder(getContext())
                            .setTitle(R.string.reset_dialog_title)
                            .setMessage(R.string.reset_dialog_message)
                            .setPositiveButton(R.string.reset_dialog_positive,
                                    new android.content.DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(android.content.DialogInterface dialog,
                                                int which) {
                                            GlobalSettings.clearGlobalSettings(getContext());
                                            final AngleRuleHelper angleRuleHelper = new AngleRuleHelper(
                                                    getContext());
                                            mGlobalSettings = new GlobalSettings(getContext(),
                                                    angleRuleHelper.getPackageNamesForAngle(),
                                                    angleRuleHelper.getPackageNamesForNative());
                                        }
                                    })
                            .setNegativeButton(R.string.reset_dialog_negative, null)
                            .show();
                    return true;
                }
            });
        }

        final Context context = preferenceCategory.getContext();
        for (PackageInfo packageInfo : mInstalledPackages)
        {
            final ListPreference listPreference = new ListPreference(context);
            initListPreference(packageInfo, listPreference);
            preferenceCategory.addPreference(listPreference);
        }
    }

    private void updateInstalledAppsListPreference()
    {
        final String selectDriverCategoryKey = getContext().getString(
                R.string.pref_key_select_opengl_driver_category);
        final PreferenceCategory preferenceCategory = (PreferenceCategory) findPreference(
                selectDriverCategoryKey);

        for (int i = 0; i < preferenceCategory.getPreferenceCount(); i++)
        {
            Preference preference = preferenceCategory.getPreference(i);
            if (preference instanceof ListPreference)
            {
                ListPreference listPreference = (ListPreference) preference;
                // Update value from current GlobalSettings
                String packageName = listPreference.getKey();
                String value = mGlobalSettings.getDriverSelectionValue(packageName);
                listPreference.setValue(value);
                listPreference.setSummary(value);
            }
        }
    }

    private void queryInstalledPackages()
    {
        mInstalledPackages.clear();

        final Intent mainIntent = new Intent(Intent.ACTION_MAIN, null);
        mainIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        final PackageManager packageManager = getActivity().getPackageManager();

        List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(mainIntent, 0);

        for (ResolveInfo resolveInfo : resolveInfos)
        {
            final String packageName = resolveInfo.activityInfo.packageName;
            if (DEBUG)
            {
                Log.d(TAG, "Package found: " + packageName);
            }
            try
            {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                {
                    PackageInfo packageInfo = packageManager.getPackageInfo(packageName,
                            PackageManager.PackageInfoFlags.of(0));
                    mInstalledPackages.add(packageInfo);
                }
                else
                {
                    PackageInfo packageInfo = packageManager.getPackageInfo(packageName, 0);
                    mInstalledPackages.add(packageInfo);
                }
            }
            catch (NameNotFoundException e)
            {
                Log.v(TAG, "Package not found: " + packageName);
            }
        }

        Collections.sort(mInstalledPackages, displayNameComparator);
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

    private void initListPreference(PackageInfo packageInfo, ListPreference listPreference)
    {
        final String packageName = packageInfo.packageName;
        listPreference.setKey(packageName);
        listPreference.setTitle(getAppName(packageInfo));

        if (mIsAngleSystemDriver) {
            // if ANGLE is the system driver set by the ro property, then we disable the option and
            // show all apps using ANGLE, because both "native" and "angle" options will ends up
            // loading ANGLE, allowing users to choose "native" but still loads ANGLE will create
            // more confusion.
            listPreference.setEnabled(false);
            listPreference.setSummary(GlobalSettings.DRIVER_SELECTION_ANGLE);
            listPreference.setValue(GlobalSettings.DRIVER_SELECTION_ANGLE);
            return;
        }

        final CharSequence[] drivers = getResources().getStringArray(R.array.driver_values);
        listPreference.setEntries(drivers);
        listPreference.setEntryValues(drivers);

        // Read directly from GlobalSettings, which is the source of truth
        final String driverSelectionValue = mGlobalSettings.getDriverSelectionValue(packageName);

        listPreference.setDefaultValue(driverSelectionValue);
        listPreference.setValue(driverSelectionValue);
        listPreference.setSummary(driverSelectionValue);

        final String dialogTitle = getContext().getString(R.string.select_opengl_driver_title);
        listPreference.setDialogTitle(dialogTitle);

        listPreference.setDialogIcon(packageInfo.applicationInfo.loadIcon(getActivity().getPackageManager()));

        listPreference.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue)
            {
                final ListPreference listPreference = (ListPreference) preference;
                final String newDriverSelectionValue = newValue.toString();
                listPreference.setSummary(newDriverSelectionValue);
                mGlobalSettings.updatePackageDriverSelection(preference.getKey(), newDriverSelectionValue);
                return true;
            }
        });
    }
}

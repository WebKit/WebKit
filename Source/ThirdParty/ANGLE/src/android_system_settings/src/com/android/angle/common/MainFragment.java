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
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
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
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreference;

import com.android.angle.R;

import java.lang.reflect.Method;
import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

public class MainFragment extends PreferenceFragmentCompat implements OnSharedPreferenceChangeListener
{

    private final String TAG = this.getClass().getSimpleName();
    private final boolean DEBUG = false;

    private boolean mIsAngleSystemDriver;
    private SharedPreferences mSharedPreferences;
    private SwitchPreference mShowAngleInUseSwitchPref;

    private GlobalSettings mGlobalSettings;
    private Receiver mRefreshReceiver = new Receiver();
    private List<PackageInfo> mInstalledPackages = new ArrayList<>();
    private AngleRuleHelper angleRuleHelper;

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

        queryInstalledPackages();

        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(
                getActivity().getApplicationContext());
        final String eglDriver = getSystemProperty("ro.hardware.egl", "");
        mIsAngleSystemDriver = eglDriver.equals("angle");
        if (DEBUG)
        {
            Map<String, ?> preferences = PreferenceManager
                    .getDefaultSharedPreferences(getActivity().getApplicationContext()).getAll();
            for (String key : preferences.keySet())
            {
                Log.d(TAG, key + ", " + preferences.get(key));
            }
            Log.d(TAG, "ro.hardware.egl = " + eglDriver);
        }
        mGlobalSettings = new GlobalSettings(getContext(), mSharedPreferences, mInstalledPackages);

        createShowAngleInUseSwitchPreference();
        createInstalledAppsListPreference();
    }

    @Override
    public void onResume()
    {
        super.onResume();

        getActivity().registerReceiver(mRefreshReceiver,
                new IntentFilter(
                        getContext().getString(R.string.intent_angle_for_android_toast_message)));
        getPreferenceScreen().getSharedPreferences().registerOnSharedPreferenceChangeListener(
                listener);
        updatePreferences();
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

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key)
    {
        Log.v(TAG, "Shared preference changed: " + key);
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
        final String selectDriverCategoryKey =
                getContext().getString(R.string.pref_key_select_opengl_driver_category);
        final PreferenceCategory preferenceCategory =
                (PreferenceCategory) findPreference(selectDriverCategoryKey);
        preferenceCategory.removeAll();

        final Context context = preferenceCategory.getContext();
        for (PackageInfo packageInfo : mInstalledPackages)
        {
            final ListPreference listPreference = new ListPreference(context);
            initListPreference(packageInfo, listPreference);
            preferenceCategory.addPreference(listPreference);
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
        final String driverSelectionValue = mSharedPreferences.getString(packageName,
                GlobalSettings.DRIVER_SELECTION_DEFAULT);
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

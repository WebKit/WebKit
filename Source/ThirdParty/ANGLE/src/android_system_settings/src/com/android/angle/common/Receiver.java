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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import com.android.angle.R;

public class Receiver extends BroadcastReceiver
{
    private static final String TAG = "AngleBroadcastReceiver";
    private static final boolean DEBUG = false;

    @Override
    public void onReceive(Context context, Intent intent)
    {
        final String action = intent.getAction();
        if (DEBUG)
        {
            Log.d(TAG, "Received intent: " + action);
        }

        if (action.equals(context.getString(R.string.intent_angle_for_android_toast_message)))
        {
            Bundle results = getResultExtras(true);
            results.putString(context.getString(R.string.intent_key_a4a_toast_message),
                    context.getString(R.string.angle_in_use_toast_message));
        }
        else if (action.equals(Intent.ACTION_BOOT_COMPLETED) || action.equals(Intent.ACTION_MY_PACKAGE_REPLACED))
        {

            updateGlobalSettings(context);
        }
    }

    /**
     * Consume the results of rule parsing to populate global settings
    */
    private static void updateGlobalSettings(Context context)
    {
        // When the device boots, we want to ensure passed-in rules are applied (if defaults) AND
        // user preferences are preserved. We now rely solely on GlobalSettings as the source of
        // truth, but we merge in any defaults from AngleRuleHelper that aren't already set.

        final AngleRuleHelper angleRuleHelper = new AngleRuleHelper(context);
        new GlobalSettings(context, angleRuleHelper.getPackageNamesForAngle(),
                angleRuleHelper.getPackageNamesForNative());
    }
}

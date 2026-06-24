/*
 *  Copyright 2015 The Chromium Authors.
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.native_test;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.chromium.build.gtest_apk.TestStatusIntent;

/**
 * Broadcasts test status to any listening {@link org.chromium.test.reporter.TestStatusReceiver}.
 */
public class TestStatusReporter {
    private final Context mContext;

    public TestStatusReporter(Context c) {
        mContext = c;
    }

    public void testRunStarted(int pid) {
        sendTestRunBroadcast(TestStatusIntent.ACTION_TEST_RUN_STARTED, pid);
    }

    public void testRunFinished(int pid) {
        sendTestRunBroadcast(TestStatusIntent.ACTION_TEST_RUN_FINISHED, pid);
    }

    private void sendTestRunBroadcast(String action, int pid) {
        Intent i = new Intent(action);
        i.setType(TestStatusIntent.DATA_TYPE_RESULT);
        i.putExtra(TestStatusIntent.EXTRA_PID, pid);
        mContext.sendBroadcast(i);
    }

    public void uncaughtException(int pid, Throwable ex) {
        Intent i = new Intent(TestStatusIntent.ACTION_UNCAUGHT_EXCEPTION);
        i.setType(TestStatusIntent.DATA_TYPE_RESULT);
        i.putExtra(TestStatusIntent.EXTRA_PID, pid);
        i.putExtra(TestStatusIntent.EXTRA_STACK_TRACE, Log.getStackTraceString(ex));
        mContext.sendBroadcast(i);
    }
}

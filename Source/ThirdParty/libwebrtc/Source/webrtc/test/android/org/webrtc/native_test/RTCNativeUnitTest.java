/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.native_test;

import android.app.Activity;
import android.util.Log;
import org.chromium.build.NativeLibraries;
import org.webrtc.native_test.NativeTestWebrtc;
import org.webrtc.ContextUtils;

/**
 * Native unit test that calls ContextUtils.initialize for WebRTC.
 */
public class RTCNativeUnitTest extends NativeTestWebrtc {

    private static final String TAG = "RTCNativeUnitTest";

    private static final String LIBRARY_UNDER_TEST_NAME =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.LibraryUnderTest";

    @Override
    public void preCreate(Activity activity) {
        super.preCreate(activity);

        // For NativeActivity based tests, dependency libraries must be loaded before
        // NativeActivity::OnCreate, otherwise loading android.app.lib_name will fail
        String libraryToLoad = activity.getIntent().getStringExtra(LIBRARY_UNDER_TEST_NAME);
        loadLibraries(
                libraryToLoad != null ? new String[] {libraryToLoad} : NativeLibraries.LIBRARIES);

        ContextUtils.initialize(activity.getApplicationContext());
    }

    private void loadLibraries(String[] librariesToLoad) {
        for (String library : librariesToLoad) {
            Log.i(TAG, "loading: " + library);
            System.loadLibrary(library);
            Log.i(TAG, "loaded: " + library);
        }
    }
}
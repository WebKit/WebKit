/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.native_test;

import android.app.Application;
import android.content.Context;

/** Application class to be used by native_test apks. */
public class RTCNativeTestApplication extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        assert getBaseContext() != null;

        // This is required for Mockito to initialize mocks without running under Instrumentation.
        System.setProperty("org.mockito.android.target", getCacheDir().getPath());
    }
}
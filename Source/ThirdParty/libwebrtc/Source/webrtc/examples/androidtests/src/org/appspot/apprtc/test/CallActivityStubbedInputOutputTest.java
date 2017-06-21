/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc.test;

import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.Espresso.onView;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.support.test.espresso.IdlingPolicies;
import android.support.test.filters.LargeTest;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.InstrumentationRegistry;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import org.appspot.apprtc.CallActivity;
import org.appspot.apprtc.ConnectActivity;
import org.appspot.apprtc.R;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Used to start a loopback call with video input from file and video output also to file.
 * The test case is a building block in other testing for video quality.
 */
@RunWith(AndroidJUnit4.class)
@LargeTest
public class CallActivityStubbedInputOutputTest {
  private static final String TAG = "CallActivityStubbedInputOutputTest";

  @Rule
  public ActivityTestRule<CallActivity> rule = new ActivityTestRule<CallActivity>(
      CallActivity.class) {
    @Override
    protected Intent getActivityIntent() {
      Context context = InstrumentationRegistry.getContext();
      Intent intent = new Intent("android.intent.action.VIEW", Uri.parse("http://localhost:9999"));

      intent.putExtra(CallActivity.EXTRA_USE_VALUES_FROM_INTENT, true);

      intent.putExtra(CallActivity.EXTRA_LOOPBACK, true);
      intent.putExtra(CallActivity.EXTRA_AUDIOCODEC, "OPUS");
      intent.putExtra(CallActivity.EXTRA_VIDEOCODEC, "VP8");
      intent.putExtra(CallActivity.EXTRA_CAPTURETOTEXTURE_ENABLED, false);
      intent.putExtra(CallActivity.EXTRA_CAMERA2, false);
      intent.putExtra(CallActivity.EXTRA_ROOMID, UUID.randomUUID().toString().substring(0, 8));
      // TODO false for wstls to disable https, should be option later or if URL is http
      intent.putExtra(CallActivity.EXTRA_URLPARAMETERS,
          "debug=loopback&ts=&wshpp=localhost:8089&wstls=false");

      intent.putExtra(CallActivity.EXTRA_VIDEO_FILE_AS_CAMERA,
          Environment.getExternalStorageDirectory().getAbsolutePath()
              + "/chromium_tests_root/resources/reference_video_640x360_30fps.y4m");

      intent.putExtra(CallActivity.EXTRA_SAVE_REMOTE_VIDEO_TO_FILE,
          Environment.getExternalStorageDirectory().getAbsolutePath() + "/output.y4m");
      intent.putExtra(CallActivity.EXTRA_SAVE_REMOTE_VIDEO_TO_FILE_WIDTH, 640);
      intent.putExtra(CallActivity.EXTRA_SAVE_REMOTE_VIDEO_TO_FILE_HEIGHT, 360);

      return intent;
    }
  };

  @Test
  public void testLoopback() throws InterruptedException {
    // The time to write down the data during closing of the program can take a while.
    IdlingPolicies.setMasterPolicyTimeout(240000, TimeUnit.MILLISECONDS);

    // During the time we sleep it will record video.
    Thread.sleep(8000);

    // Click on hang-up button.
    onView(withId(R.id.button_call_disconnect)).perform(click());
  }
}

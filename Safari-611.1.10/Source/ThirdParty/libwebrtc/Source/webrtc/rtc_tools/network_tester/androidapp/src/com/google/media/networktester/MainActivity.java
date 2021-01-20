/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package com.google.media.networktester;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.widget.Button;

public class MainActivity extends Activity {
  Button startButton;
  Button stopButton;
  NetworkTester networkTester;
  Handler mainThreadHandler;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    startButton = (Button) findViewById(R.id.start_button);
    startButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        startTest();
      }
    });
    stopButton = (Button) findViewById(R.id.stop_button);
    stopButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        stopTest();
      }
    });
    mainThreadHandler = new Handler();
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  }

  private void startTest() {
    if (networkTester == null) {
      networkTester = new NetworkTester();
      networkTester.start();
    }
  }

  private void stopTest() {
    if (networkTester != null) {
      networkTester.interrupt();
      networkTester = null;
    }
  }
}

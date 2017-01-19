/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * This file and its corresponding .mm file implement a main function on Mac.
 * It's useful if you need to access a webcam in your Mac application. The code
 * forks a worker thread which runs the below ImplementThisToRunYourTest
 * function, and uses the main thread to pump messages. That way we can run our
 * code in a regular sequential fashion and still pump events, which are
 * necessary to access the webcam for instance.
 */

// Implement this method to do whatever you want to do in the worker thread.
// The argc and argv variables are the unmodified command line from main.
int ImplementThisToRunYourTest(int argc, char** argv);

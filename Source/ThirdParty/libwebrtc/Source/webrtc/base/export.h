/*
 *  Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_BASE_EXPORT_H_
#define WEBRTC_BASE_EXPORT_H_

#ifdef WEBRTC_WEBKIT_BUILD
#define WEBRTC_DYLIB_EXPORT __attribute__((visibility ("default")))
#else
#define WEBRTC_DYLIB_EXPORT
#endif

#endif

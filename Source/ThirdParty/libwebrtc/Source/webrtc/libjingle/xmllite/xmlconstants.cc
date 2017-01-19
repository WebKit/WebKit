/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/xmlconstants.h"

namespace buzz {

const char STR_EMPTY[] = "";
const char NS_XML[] = "http://www.w3.org/XML/1998/namespace";
const char NS_XMLNS[] = "http://www.w3.org/2000/xmlns/";
const char STR_XMLNS[] = "xmlns";
const char STR_XML[] = "xml";
const char STR_VERSION[] = "version";
const char STR_ENCODING[] = "encoding";

const StaticQName QN_XMLNS = { STR_EMPTY, STR_XMLNS };

}  // namespace buzz

/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Version numbers are based on the 'current library version' specified in the WebKit build rules.
   All of these methods return or take version numbers with each part shifted to the left 2 bytes.
   For example the version 1.2.3 is returned as 0x00010203 and version 200.3.5 is returned as 0x00C80305
   A version of -1 is returned if the main executable did not link against WebKit (should never happen). */

#define WEBKIT_FIRST_VERSION_WITH_3_0_CONTEXT_MENU_TAGS 0x020A0000 // 522.0.0
#define WEBKIT_FIRST_VERSION_WITH_LOCAL_RESOURCE_SECURITY_RESTRICTION 0x020A0000 // 522.0.0
#define WEBKIT_FIRST_VERSION_WITHOUT_APERTURE_QUIRK 0x020A0000 // 522.0.0
#define WEBKIT_FIRST_VERSION_WITHOUT_QUICKBOOKS_QUIRK 0x020A0000 // 522.0.0
#define WEBKIT_FIRST_VERSION_WITHOUT_VITALSOURCE_QUIRK 0x020A0000 // 522.0.0

#ifdef __cplusplus
extern "C" {
#endif

BOOL WebKitLinkedOnOrAfter(int version);
int WebKitLinkTimeVersion(void);
int WebKitRunTimeVersion(void);

#ifdef __cplusplus
}
#endif

/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RuntimeApplicationChecks.h"


namespace WebCore {

bool applicationIsAppleMail()
{
    static const bool isAppleMail = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mail"];
    return isAppleMail;
}

bool applicationIsSafari()
{
    static const bool isSafari = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Safari"];
    return isSafari;
}

bool applicationIsMicrosoftMessenger()
{
    static bool isMicrosoftMessenger = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.microsoft.Messenger"];
    return isMicrosoftMessenger;
}

bool applicationIsAdobeInstaller()
{
    static bool isAdobeInstaller = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.adobe.Installers.Setup"];
    return isAdobeInstaller;
}
    
bool applicationIsAOLInstantMessenger()
{
    static bool isAOLInstantMessenger = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.aol.aim.desktop"];
    return isAOLInstantMessenger;
}

} // namespace WebCore

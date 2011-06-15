/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreArgumentCoders.h"

#include <WebCore/PluginData.h>

using namespace WebCore;
using namespace WebKit;

namespace CoreIPC {
    
void ArgumentCoder<MimeClassInfo>::encode(ArgumentEncoder* encoder, const MimeClassInfo& mimeClassInfo)
{
    encoder->encode(mimeClassInfo.type);
    encoder->encode(mimeClassInfo.desc);
    encoder->encode(mimeClassInfo.extensions);
}

bool ArgumentCoder<MimeClassInfo>::decode(ArgumentDecoder* decoder, MimeClassInfo& mimeClassInfo)
{
    if (!decoder->decode(mimeClassInfo.type))
        return false;
    if (!decoder->decode(mimeClassInfo.desc))
        return false;
    if (!decoder->decode(mimeClassInfo.extensions))
        return false;

    return true;
}


void ArgumentCoder<PluginInfo>::encode(ArgumentEncoder* encoder, const PluginInfo& pluginInfo)
{
    encoder->encode(pluginInfo.name);
    encoder->encode(pluginInfo.file);
    encoder->encode(pluginInfo.desc);
    encoder->encode(pluginInfo.mimes);
}
    
bool ArgumentCoder<PluginInfo>::decode(ArgumentDecoder* decoder, PluginInfo& pluginInfo)
{
    if (!decoder->decode(pluginInfo.name))
        return false;
    if (!decoder->decode(pluginInfo.file))
        return false;
    if (!decoder->decode(pluginInfo.desc))
        return false;
    if (!decoder->decode(pluginInfo.mimes))
        return false;

    return true;
}


void ArgumentCoder<HTTPHeaderMap>::encode(ArgumentEncoder* encoder, const HTTPHeaderMap& headerMap)
{
    encoder->encode(static_cast<const HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
}

bool ArgumentCoder<HTTPHeaderMap>::decode(ArgumentDecoder* decoder, HTTPHeaderMap& headerMap)
{
    return decoder->decode(static_cast<HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
}


void ArgumentCoder<AuthenticationChallenge>::encode(ArgumentEncoder* encoder, const AuthenticationChallenge& challenge)
{
    encoder->encode(challenge.protectionSpace());
    encoder->encode(challenge.proposedCredential());
    encoder->encode(challenge.previousFailureCount());
    encoder->encode(challenge.failureResponse());
    encoder->encode(challenge.error());
}

bool ArgumentCoder<AuthenticationChallenge>::decode(ArgumentDecoder* decoder, AuthenticationChallenge& challenge)
{    
    ProtectionSpace protectionSpace;
    if (!decoder->decode(protectionSpace))
        return false;

    Credential proposedCredential;
    if (!decoder->decode(proposedCredential))
        return false;

    unsigned previousFailureCount;    
    if (!decoder->decode(previousFailureCount))
        return false;

    ResourceResponse failureResponse;
    if (!decoder->decode(failureResponse))
        return false;

    ResourceError error;
    if (!decoder->decode(error))
        return false;
    
    challenge = AuthenticationChallenge(protectionSpace, proposedCredential, previousFailureCount, failureResponse, error);
    return true;
}


void ArgumentCoder<ProtectionSpace>::encode(ArgumentEncoder* encoder, const ProtectionSpace& space)
{
    encoder->encode(space.host());
    encoder->encode(space.port());
    encoder->encodeEnum(space.serverType());
    encoder->encode(space.realm());
    encoder->encodeEnum(space.authenticationScheme());
}

bool ArgumentCoder<ProtectionSpace>::decode(ArgumentDecoder* decoder, ProtectionSpace& space)
{
    String host;
    if (!decoder->decode(host))
        return false;

    int port;
    if (!decoder->decode(port))
        return false;

    ProtectionSpaceServerType serverType;
    if (!decoder->decodeEnum(serverType))
        return false;

    String realm;
    if (!decoder->decode(realm))
        return false;
    
    ProtectionSpaceAuthenticationScheme authenticationScheme;
    if (!decoder->decodeEnum(authenticationScheme))
        return false;

    space = ProtectionSpace(host, port, serverType, realm, authenticationScheme);
    return true;
}

void ArgumentCoder<Credential>::encode(ArgumentEncoder* encoder, const Credential& credential)
{
    encoder->encode(credential.user());
    encoder->encode(credential.password());
    encoder->encodeEnum(credential.persistence());
}

bool ArgumentCoder<Credential>::decode(ArgumentDecoder* decoder, Credential& credential)
{
    String user;
    if (!decoder->decode(user))
        return false;

    String password;
    if (!decoder->decode(password))
        return false;

    CredentialPersistence persistence;
    if (!decoder->decodeEnum(persistence))
        return false;
    
    credential = Credential(user, password, persistence);
    return true;
}

static void encodeImage(ArgumentEncoder* encoder, Image* image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(image->size(), ShareableBitmap::SupportsAlpha);
    bitmap->createGraphicsContext()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder->encode(handle);
}

static bool decodeImage(ArgumentDecoder* decoder, RefPtr<Image>& image)
{
    ShareableBitmap::Handle handle;
    if (!decoder->decode(handle))
        return false;
    
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return false;
    image = bitmap->createImage();
    if (!image)
        return false;
    return true;
}

#if USE(LAZY_NATIVE_CURSOR)
void ArgumentCoder<Cursor>::encode(ArgumentEncoder* encoder, const Cursor& cursor)
{
    encoder->encodeEnum(cursor.type());
        
    if (cursor.type() != Cursor::Custom)
        return;

    encodeImage(encoder, cursor.image());
    encoder->encode(cursor.hotSpot());
}

bool ArgumentCoder<Cursor>::decode(ArgumentDecoder* decoder, Cursor& cursor)
{
    Cursor::Type type;
    if (!decoder->decodeEnum(type))
        return false;

    if (type > Cursor::Custom)
        return false;

    if (type != Cursor::Custom) {
        cursor = Cursor::fromType(type);
        return true;
    }

    RefPtr<Image> image;
    if (!decodeImage(decoder, image))
        return false;

    IntPoint hotSpot;
    if (!decoder->decode(hotSpot))
        return false;

    if (!image->rect().contains(hotSpot))
        return false;

    cursor = Cursor(image.get(), hotSpot);
    return true;
}
#endif // USE(LAZY_NATIVE_CURSOR)


void ArgumentCoder<WindowFeatures>::encode(ArgumentEncoder* encoder, const WindowFeatures& windowFeatures)
{
    encoder->encode(windowFeatures.x);
    encoder->encode(windowFeatures.y);
    encoder->encode(windowFeatures.width);
    encoder->encode(windowFeatures.height);
    encoder->encode(windowFeatures.xSet);
    encoder->encode(windowFeatures.ySet);
    encoder->encode(windowFeatures.widthSet);
    encoder->encode(windowFeatures.heightSet);
    encoder->encode(windowFeatures.menuBarVisible);
    encoder->encode(windowFeatures.statusBarVisible);
    encoder->encode(windowFeatures.toolBarVisible);
    encoder->encode(windowFeatures.locationBarVisible);
    encoder->encode(windowFeatures.scrollbarsVisible);
    encoder->encode(windowFeatures.resizable);
    encoder->encode(windowFeatures.fullscreen);
    encoder->encode(windowFeatures.dialog);
}

bool ArgumentCoder<WindowFeatures>::decode(ArgumentDecoder* decoder, WindowFeatures& windowFeatures)
{
    if (!decoder->decode(windowFeatures.x))
        return false;
    if (!decoder->decode(windowFeatures.y))
        return false;
    if (!decoder->decode(windowFeatures.width))
        return false;
    if (!decoder->decode(windowFeatures.height))
        return false;
    if (!decoder->decode(windowFeatures.xSet))
        return false;
    if (!decoder->decode(windowFeatures.ySet))
        return false;
    if (!decoder->decode(windowFeatures.widthSet))
        return false;
    if (!decoder->decode(windowFeatures.heightSet))
        return false;
    if (!decoder->decode(windowFeatures.menuBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.statusBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.toolBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.locationBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.scrollbarsVisible))
        return false;
    if (!decoder->decode(windowFeatures.resizable))
        return false;
    if (!decoder->decode(windowFeatures.fullscreen))
        return false;
    if (!decoder->decode(windowFeatures.dialog))
        return false;
    return true;
}


void ArgumentCoder<Color>::encode(ArgumentEncoder* encoder, const Color& color)
{
    if (!color.isValid()) {
        encoder->encodeBool(false);
        return;
    }

    encoder->encodeBool(true);
    encoder->encode(color.rgb());
}

bool ArgumentCoder<Color>::decode(ArgumentDecoder* decoder, Color& color)
{
    bool isValid;
    if (!decoder->decode(isValid))
        return false;

    if (!isValid) {
        color = Color();
        return true;
    }

    RGBA32 rgba;
    if (!decoder->decode(rgba))
        return false;

    color = Color(rgba);
    return true;
}

} // namespace CoreIPC

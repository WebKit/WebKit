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
#include "WKIconDatabase.h"

#include "APIData.h"
#include "WKAPICast.h"
#include "WebIconDatabase.h"

using namespace WebKit;

WKTypeID WKIconDatabaseGetTypeID()
{
    return toAPI(WebIconDatabase::APIType);
}

void WKIconDatabaseSetIconDatabaseClient(WKIconDatabaseRef iconDatabaseRef, const WKIconDatabaseClientBase* wkClient)
{
    toImpl(iconDatabaseRef)->initializeIconDatabaseClient(wkClient);
}

void WKIconDatabaseRetainIconForURL(WKIconDatabaseRef iconDatabaseRef, WKURLRef pageURLRef)
{
    toImpl(iconDatabaseRef)->retainIconForPageURL(toWTFString(pageURLRef));
}

void WKIconDatabaseReleaseIconForURL(WKIconDatabaseRef iconDatabaseRef, WKURLRef pageURLRef)
{
    toImpl(iconDatabaseRef)->releaseIconForPageURL(toWTFString(pageURLRef));
}

void WKIconDatabaseSetIconDataForIconURL(WKIconDatabaseRef iconDatabaseRef, WKDataRef iconDataRef, WKURLRef iconURLRef)
{
    toImpl(iconDatabaseRef)->setIconDataForIconURL(toImpl(iconDataRef)->dataReference(), toWTFString(iconURLRef));
}

void WKIconDatabaseSetIconURLForPageURL(WKIconDatabaseRef iconDatabaseRef, WKURLRef iconURLRef, WKURLRef pageURLRef)
{
    toImpl(iconDatabaseRef)->setIconURLForPageURL(toWTFString(iconURLRef), toWTFString(pageURLRef));
}

WKURLRef WKIconDatabaseCopyIconURLForPageURL(WKIconDatabaseRef iconDatabaseRef, WKURLRef pageURLRef)
{
    String iconURLString;
    toImpl(iconDatabaseRef)->synchronousIconURLForPageURL(toWTFString(pageURLRef), iconURLString);
    return toCopiedURLAPI(iconURLString);
}

WKDataRef WKIconDatabaseCopyIconDataForPageURL(WKIconDatabaseRef iconDatabaseRef, WKURLRef pageURL)
{
    return toAPI(toImpl(iconDatabaseRef)->iconDataForPageURL(toWTFString(pageURL)).leakRef());
}

void WKIconDatabaseEnableDatabaseCleanup(WKIconDatabaseRef iconDatabaseRef)
{
    toImpl(iconDatabaseRef)->enableDatabaseCleanup();
}

void WKIconDatabaseRemoveAllIcons(WKIconDatabaseRef iconDatabaseRef)
{
    toImpl(iconDatabaseRef)->removeAllIcons();
}

void WKIconDatabaseCheckIntegrityBeforeOpening(WKIconDatabaseRef iconDatabaseRef)
{
    toImpl(iconDatabaseRef)->checkIntegrityBeforeOpening();
}

void WKIconDatabaseClose(WKIconDatabaseRef iconDatabaseRef)
{
    toImpl(iconDatabaseRef)->close();
}

/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebKitLogging.h"

KXCLogChannel WebKitLogTiming =       		{ 0x00000020, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogLoading =      		{ 0x00000040, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogFontCache =    		{ 0x00000100, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogFontSubstitution =	{ 0x00000200, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogDownload =     		{ 0x00000800, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogDocumentLoad =		{ 0x00001000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogPlugins =		{ 0x00002000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogEvents =       		{ 0x00010000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogView =         		{ 0x00020000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogRedirect =     		{ 0x00040000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogPageCache =              { 0x00080000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogCacheSizes =             { 0x00100000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogFormDelegate = 	        { 0x00200000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogFileDatabaseActivity =   { 0x00400000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogHistory =                { 0x00800000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogBindings =               { 0x01000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogFontSelection =	        { 0x02000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogEncoding =               { 0x04000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogLiveConnect =            { 0x08000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogBackForward =            { 0x10000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogProgress =               { 0x20000000, "WebKitLogLevel", KXCLogChannelOff };
KXCLogChannel WebKitLogPluginEvents =           { 0x40000000, "WebKitLogLevel", KXCLogChannelOff };

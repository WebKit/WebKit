//
//  WebKitLogging.m
//  WebKit
//
//  Created by Darin Adler on Sun Sep 08 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebKitLogging.h"

WebLogChannel WebKitLogTiming =       		{ 0x00000020, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogLoading =      		{ 0x00000040, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogFontCache =    		{ 0x00000100, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogFontSubstitution =	{ 0x00000200, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogDownload =     		{ 0x00000800, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogDocumentLoad =		{ 0x00001000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogPlugins =		{ 0x00002000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogEvents =       		{ 0x00010000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogView =         		{ 0x00020000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogRedirect =     		{ 0x00040000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogPageCache =              { 0x00080000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogCacheSizes =             { 0x00100000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogFormDelegate = 	        { 0x00200000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogFileDatabaseActivity =   { 0x00400000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogHistory =                { 0x00800000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogBindings =               { 0x01000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogFontSelection =	        { 0x02000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogEncoding =               { 0x04000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogLiveConnect =            { 0x08000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogBackForward =            { 0x10000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogProgress =               { 0x20000000, "WebKitLogLevel", WebLogChannelUninitialized };
WebLogChannel WebKitLogPluginEvents =           { 0x40000000, "WebKitLogLevel", WebLogChannelUninitialized };


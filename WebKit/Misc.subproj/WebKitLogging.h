//
//  WebKitLogging.h
//  WebKit
//
//  Created by Darin Adler on Sun Sep 08 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebAssertions.h>

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WebKitLog
#endif

extern WebLogChannel WebKitLogTiming;
extern WebLogChannel WebKitLogLoading;
extern WebLogChannel WebKitLogFontCache;
extern WebLogChannel WebKitLogFontSubstitution;
extern WebLogChannel WebKitLogFontSelection;
extern WebLogChannel WebKitLogDownload;
extern WebLogChannel WebKitLogDocumentLoad;
extern WebLogChannel WebKitLogPlugins;
extern WebLogChannel WebKitLogEvents;
extern WebLogChannel WebKitLogView;
extern WebLogChannel WebKitLogRedirect;
extern WebLogChannel WebKitLogPageCache;
extern WebLogChannel WebKitLogCacheSizes;
extern WebLogChannel WebKitLogFormDelegate;
extern WebLogChannel WebKitLogFileDatabaseActivity;
extern WebLogChannel WebKitLogHistory;
extern WebLogChannel WebKitLogBindings;
extern WebLogChannel WebKitLogEncoding;
extern WebLogChannel WebKitLogLiveConnect;
extern WebLogChannel WebKitLogBackForward;
extern WebLogChannel WebKitLogProgress;
extern WebLogChannel WebKitLogPluginEvents;

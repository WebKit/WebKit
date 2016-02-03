/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebCoreNSURLSession_h
#define WebCoreNSURLSession_h

#import "CachedRawResource.h"
#import "CachedResourceHandle.h"
#import <Foundation/NSURLSession.h>
#import <wtf/HashSet.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

@class NSNetService;
@class NSOperationQueue;
@class NSURL;
@class NSURLRequest;
@class NSURLResponse;
@class WebCoreNSURLSessionDataTask;

namespace WebCore {
class CachedResourceLoader;
class CachedResourceRequest;
class WebCoreNSURLSessionDataTaskClient;
}

NS_ASSUME_NONNULL_BEGIN

WEBCORE_EXPORT @interface WebCoreNSURLSession : NSObject {
    RefPtr<WebCore::CachedResourceLoader> _loader;
    RetainPtr<id<NSURLSessionDelegate>> _delegate;
    RetainPtr<NSOperationQueue> _queue;
    NSString *_sessionDescription;
    HashSet<RetainPtr<WebCoreNSURLSessionDataTask>> _dataTasks;
    BOOL _invalidated;
    NSUInteger _nextTaskIdentifier;
}
- (id)initWithResourceLoader:(WebCore::CachedResourceLoader&)loader delegate:(id<NSURLSessionTaskDelegate>)delegate delegateQueue:(NSOperationQueue*)queue;
@property (readonly, retain) NSOperationQueue *delegateQueue;
@property (nullable, readonly, retain) id <NSURLSessionDelegate> delegate;
@property (readonly, copy) NSURLSessionConfiguration *configuration;
@property (copy) NSString *sessionDescription;
- (void)finishTasksAndInvalidate;
- (void)invalidateAndCancel;

- (void)resetWithCompletionHandler:(void (^)(void))completionHandler;
- (void)flushWithCompletionHandler:(void (^)(void))completionHandler;
- (void)getTasksWithCompletionHandler:(void (^)(NSArray<NSURLSessionDataTask *> *dataTasks, NSArray<NSURLSessionUploadTask *> *uploadTasks, NSArray<NSURLSessionDownloadTask *> *downloadTasks))completionHandler;
- (void)getAllTasksWithCompletionHandler:(void (^)(NSArray<__kindof NSURLSessionTask *> *tasks))completionHandler;

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData;
- (NSURLSessionUploadTask *)uploadTaskWithStreamedRequest:(NSURLRequest *)request;
- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request;
- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url;
- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData;
- (NSURLSessionStreamTask *)streamTaskWithHostName:(NSString *)hostname port:(NSInteger)port;
- (NSURLSessionStreamTask *)streamTaskWithNetService:(NSNetService *)service;
@end

@interface WebCoreNSURLSession (NSURLSessionAsynchronousConvenience)
- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(nullable NSData *)bodyData completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURL * location, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url completionHandler:(void (^)(NSURL * location, NSURLResponse * response, NSError * error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData completionHandler:(void (^)(NSURL * location, NSURLResponse * response, NSError * error))completionHandler;
@end

@interface WebCoreNSURLSessionDataTask : NSObject {
    WebCoreNSURLSession *_session;
    std::unique_ptr<WebCore::CachedResourceRequest> _request;
    std::unique_ptr<WebCore::WebCoreNSURLSessionDataTaskClient> _client;
    WebCore::CachedResourceHandle<WebCore::CachedRawResource> _resource;
    RetainPtr<NSURLResponse> _response;
    NSUInteger _taskIdentifier;
    NSURLRequest *_originalRequest;
    NSURLRequest *_currentRequest;
    int64_t _countOfBytesReceived;
    int64_t _countOfBytesSent;
    int64_t _countOfBytesExpectedToSend;
    int64_t _countOfBytesExpectedToReceive;
    NSURLSessionTaskState _state;
    NSError *_error;
    NSString *_taskDescription;
    float _priority;
}
@property (readwrite) NSUInteger taskIdentifier;
@property (readwrite, copy) NSURLRequest *originalRequest;
@property (readwrite, copy) NSURLRequest *currentRequest;
@property (readonly, copy) NSURLResponse *response;
@property (readwrite) int64_t countOfBytesReceived;
@property (readwrite) int64_t countOfBytesSent;
@property (readwrite) int64_t countOfBytesExpectedToSend;
@property (readwrite) int64_t countOfBytesExpectedToReceive;
@property (readwrite) NSURLSessionTaskState state;
@property (readwrite, copy) NSError *error;
@property (copy) NSString *taskDescription;
@property float priority;
- (void)cancel;
- (void)suspend;
- (void)resume;
@end

NS_ASSUME_NONNULL_END

#endif

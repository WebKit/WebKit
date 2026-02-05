/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCoreNSURLSession.h"

#import "CachedResourceRequest.h"
#import "ParsedRequestRange.h"
#import "PlatformMediaResourceLoader.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "WebCoreObjCExtras.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/FunctionDispatcher.h>
#import <wtf/Lock.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>

using namespace WebCore;
using namespace WTF;

#pragma mark - Private declarations

NS_ASSUME_NONNULL_BEGIN

static NSDate * __nullable networkLoadMetricsDate(MonotonicTime time)
{
    if (!time)
        return nil;
    NSTimeInterval value = time.approximateWallTime().secondsSinceEpoch().seconds();
    if (value <= 0)
        return nil;
    return [NSDate dateWithTimeIntervalSince1970:value];
}

@interface WebCoreNSURLSessionTaskTransactionMetrics : NSObject
- (instancetype)_initWithMetrics:(WebCore::NetworkLoadMetrics&&)metrics onTarget:(GuaranteedSerialFunctionDispatcher*)targetDispatcher;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *fetchStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *domainLookupStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *domainLookupEndDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *connectStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *secureConnectionStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *connectEndDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *requestStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *responseStartDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSDate *responseEndDate SUPPRESS_UNRETAINED_MEMBER;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (nullable, copy, readonly) NSString *networkProtocolName SUPPRESS_UNRETAINED_MEMBER;
@property (assign, readonly, getter=isReusedConnection) BOOL reusedConnection;
@property (readonly, getter=isCellular) BOOL cellular;
@property (readonly, getter=isExpensive) BOOL expensive;
@property (readonly, getter=isConstrained) BOOL constrained;
@property (readonly, getter=isMultipath) BOOL multipath;
@property (assign, readonly) nw_connection_privacy_stance_t _privacyStance;
@end

@implementation WebCoreNSURLSessionTaskTransactionMetrics {
    WebCore::NetworkLoadMetrics _metrics;
    RefPtr<GuaranteedSerialFunctionDispatcher> _targetDispatcher;
}

- (instancetype)_initWithMetrics:(WebCore::NetworkLoadMetrics&&)metrics onTarget:(GuaranteedSerialFunctionDispatcher*)dispatcher
{
    assertIsCurrent(*dispatcher);

    if (!(self = [super init]))
        return nil;
    _metrics = WTF::move(metrics);
    _targetDispatcher = dispatcher;
    return self;
}

- (void)dealloc
{
    _targetDispatcher->dispatch([metrics = WTF::move(_metrics)] { });

    [super dealloc];
}

@dynamic fetchStartDate;
- (nullable NSDate *)fetchStartDate
{
    return networkLoadMetricsDate(_metrics.fetchStart);
}

@dynamic domainLookupStartDate;
- (nullable NSDate *)domainLookupStartDate
{
    return networkLoadMetricsDate(_metrics.domainLookupStart);
}

@dynamic domainLookupEndDate;
- (nullable NSDate *)domainLookupEndDate
{
    return networkLoadMetricsDate(_metrics.domainLookupEnd);
}

@dynamic connectStartDate;
- (nullable NSDate *)connectStartDate
{
    return networkLoadMetricsDate(_metrics.connectStart);
}

@dynamic secureConnectionStartDate;
- (nullable NSDate *)secureConnectionStartDate
{
    if (_metrics.secureConnectionStart == reusedTLSConnectionSentinel)
        return nil;
    return networkLoadMetricsDate(_metrics.secureConnectionStart);
}

@dynamic connectEndDate;
- (nullable NSDate *)connectEndDate
{
    return networkLoadMetricsDate(_metrics.connectEnd);
}

@dynamic requestStartDate;
- (nullable NSDate *)requestStartDate
{
    return networkLoadMetricsDate(_metrics.requestStart);
}

@dynamic responseStartDate;
- (nullable NSDate *)responseStartDate
{
    return networkLoadMetricsDate(_metrics.responseStart);
}

@dynamic responseEndDate;
- (nullable NSDate *)responseEndDate
{
    return networkLoadMetricsDate(_metrics.responseEnd);
}

@dynamic networkProtocolName;
- (nullable NSString *)networkProtocolName
{
    return _metrics.protocol.createNSString().autorelease();
}

@dynamic reusedConnection;
- (BOOL)isReusedConnection
{
    return _metrics.isReusedConnection;
}

@dynamic _privacyStance;
- (nw_connection_privacy_stance_t)_privacyStance
{
    auto toConnectionPrivacyStance = [] (WebCore::PrivacyStance privacyStance) {
        switch (privacyStance) {
        case WebCore::PrivacyStance::Unknown:
            return nw_connection_privacy_stance_unknown;
        case WebCore::PrivacyStance::NotEligible:
            return nw_connection_privacy_stance_not_eligible;
        case WebCore::PrivacyStance::Proxied:
            return nw_connection_privacy_stance_proxied;
        case WebCore::PrivacyStance::Failed:
            return nw_connection_privacy_stance_failed;
        case WebCore::PrivacyStance::Direct:
            return nw_connection_privacy_stance_direct;
        case WebCore::PrivacyStance::FailedUnreachable:
#if defined(NW_CONNECTION_HAS_PRIVACY_STANCE_FAILED_UNREACHABLE)
            return nw_connection_privacy_stance_failed_unreachable;
#else
            return nw_connection_privacy_stance_unknown;
#endif
        }
        ASSERT_NOT_REACHED();
        return nw_connection_privacy_stance_unknown;
    };
    return toConnectionPrivacyStance(_metrics.privacyStance);
}

@dynamic cellular;
- (BOOL)cellular
{
    return _metrics.cellular;
}

@dynamic expensive;
- (BOOL)expensive
{
    return _metrics.expensive;
}

@dynamic constrained;
- (BOOL)constrained
{
    return _metrics.constrained;
}

@dynamic multipath;
- (BOOL)multipath
{
    return _metrics.multipath;
}

@end

@interface WebCoreNSURLSessionTaskMetrics : NSObject
- (instancetype)_initWithMetrics:(WebCore::NetworkLoadMetrics&&)metrics onTarget:(nonnull GuaranteedSerialFunctionDispatcher *)targetDispatcher;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (copy, readonly) NSArray<NSURLSessionTaskTransactionMetrics *> *transactionMetrics SUPPRESS_UNRETAINED_MEMBER;
@end

@implementation WebCoreNSURLSessionTaskMetrics {
    RetainPtr<WebCoreNSURLSessionTaskTransactionMetrics> _transactionMetrics;
    RefPtr<GuaranteedSerialFunctionDispatcher> _targetDispatcher;
}

- (instancetype)_initWithMetrics:(WebCore::NetworkLoadMetrics&&)metrics onTarget:(nonnull GuaranteedSerialFunctionDispatcher *)targetDispatcher
{
    assertIsCurrent(*targetDispatcher);

    if (!(self = [super init]))
        return nil;
    _targetDispatcher = targetDispatcher;
    _transactionMetrics = adoptNS([[WebCoreNSURLSessionTaskTransactionMetrics alloc] _initWithMetrics:WTF::move(metrics) onTarget:targetDispatcher]);
    return self;
}

- (void)dealloc
{
    _targetDispatcher->dispatch([metrics = WTF::move(_transactionMetrics)] { });

    [super dealloc];
}

@dynamic transactionMetrics;
- (NSArray<NSURLSessionTaskTransactionMetrics *> *)transactionMetrics
{
    // TODO: This is likely not thread-safe. How to handle this object?
    // Accessed from delegate thread.
    return @[ (NSURLSessionTaskTransactionMetrics *)self->_transactionMetrics.get() ];
}

@end

@interface WebCoreNSURLSession ()
@property (readonly) PlatformMediaResourceLoader& loader;
@property (readwrite, retain) id<NSURLSessionTaskDelegate> delegate;
- (void)taskCompleted:(WebCoreNSURLSessionDataTask *)task;
- (void)addDelegateOperation:(Function<void()>&&)operation;
- (void)task:(WebCoreNSURLSessionDataTask *)task didReceiveCORSAccessCheckResult:(BOOL)result;
- (void)task:(WebCoreNSURLSessionDataTask *)task addSecurityOrigin:(Ref<WebCore::SecurityOrigin>&&)origin;
- (WebCore::RangeResponseGenerator&)rangeResponseGenerator;
@end

@interface WebCoreNSURLSessionDataTask ()
- (id)initWithSession:(WebCoreNSURLSession *)session identifier:(NSUInteger)identifier request:(NSURLRequest *)request targetDispatcher:(GuaranteedSerialFunctionDispatcher *)targetDispatcher;
- (void)_cancel;
/* FIXME: This is a safer cpp false positive (rdar://161063702). */
@property (assign) WebCoreNSURLSession * _Nullable session SUPPRESS_UNRETAINED_MEMBER;

@end

NS_ASSUME_NONNULL_END

#pragma mark - WebCoreNSURLSession

@implementation WebCoreNSURLSession
@synthesize invalidated = _invalidated;
@synthesize corsResults = _corsResults;

- (id)initWithResourceLoader:(PlatformMediaResourceLoader&)loader delegate:(id<NSURLSessionTaskDelegate>)inDelegate delegateQueue:(NSOperationQueue*)inQueue
{
    ASSERT(isMainThread());
    self = [super init];
    if (!self)
        return nil;

    ASSERT(_corsResults == WebCoreNSURLSessionCORSAccessCheckResults::Unknown);
    ASSERT(!_invalidated);

    _loader = loader;
    _targetDispatcher = RefPtr { _loader }->targetDispatcher();
    self.delegate = inDelegate;
    _queue = inQueue ? inQueue : [NSOperationQueue mainQueue];
    _internalQueue = WorkQueue::create("WebCoreNSURLSession _internalQueue"_s);
    _targetDispatcher->dispatch([strongSelf = retainPtr(self)] {
        strongSelf->_rangeResponseGenerator = RangeResponseGenerator::create(Ref { *strongSelf->_targetDispatcher });
    });

    return self;
}

- (void)dealloc
{
    {
        Locker<Lock> locker(_dataTasksLock);
        _targetDispatcher->dispatch([dataTasks = WTF::move(_dataTasks), rangeResponseGenerator = WTF::move(_rangeResponseGenerator)] () mutable {
            for (auto&& task : dataTasks)
                [task setSession:nil];
        });
    }

    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    UNUSED_PARAM(zone);
    return [self retain];
}

#pragma mark - Internal Methods

- (void)taskCompleted:(WebCoreNSURLSessionDataTask *)task
{
    assertIsCurrent(*_targetDispatcher);
    task.session = nil;

    {
        Locker<Lock> locker(_dataTasksLock);

        ASSERT(_dataTasks.contains(task));
        _dataTasks.remove(task);
        if (!_dataTasks.isEmpty() || !self.invalidated)
            return;
    }

    [self addDelegateOperation:[strongSelf = retainPtr(self)] {
        RetainPtr<id<NSURLSessionDelegate>> delegate = strongSelf.get().delegate;
        if ([delegate respondsToSelector:@selector(URLSession:didBecomeInvalidWithError:)])
            [delegate URLSession:(NSURLSession *)strongSelf.get() didBecomeInvalidWithError:nil];
    }];
}

- (void)addDelegateOperation:(Function<void()>&&)function
{
    RetainPtr<NSBlockOperation> operation = [NSBlockOperation blockOperationWithBlock:makeBlockPtr(WTF::move(function)).get()];
    RefPtr { _internalQueue }->dispatch([strongSelf = retainPtr(self), operation = WTF::move(operation)] {
        [retainPtr(strongSelf.get().delegateQueue) addOperation:operation.get()];
        [operation waitUntilFinished];
    });
}

- (void)task:(WebCoreNSURLSessionDataTask *)task didReceiveCORSAccessCheckResult:(BOOL)result
{
    assertIsCurrent(*_targetDispatcher);
    UNUSED_PARAM(task);
    if (!result)
        self.corsResults = WebCoreNSURLSessionCORSAccessCheckResults::Fail;
    else if (self.corsResults != WebCoreNSURLSessionCORSAccessCheckResults::Fail)
        self.corsResults = WebCoreNSURLSessionCORSAccessCheckResults::Pass;
}

- (void)task:(WebCoreNSURLSessionDataTask *)task addSecurityOrigin:(Ref<WebCore::SecurityOrigin>&&)origin
{
    assertIsCurrent(*_targetDispatcher);
    UNUSED_PARAM(task);
    Locker<Lock> locker(_dataTasksLock);
    _origins.add(WTF::move(origin));
}

- (WebCore::RangeResponseGenerator&)rangeResponseGenerator
{
    assertIsCurrent(*_targetDispatcher);
    return *_rangeResponseGenerator;
}

#pragma mark - NSURLSession API
@dynamic delegate;
- (__nullable id<NSURLSessionDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id<NSURLSessionDelegate>)delegate
{
    _delegate = delegate;
}

@dynamic delegateQueue;
- (NSOperationQueue *)delegateQueue
{
    return _queue.get();
}

@dynamic configuration;
- (NSURLSessionConfiguration *)configuration
{
    return _configuration.get();
}

- (NSString *)sessionDescription
{
    return _sessionDescription.get();
}

- (void)setSessionDescription:(NSString *)sessionDescription
{
    _sessionDescription = adoptNS([sessionDescription copy]);
}

@dynamic loader;
- (PlatformMediaResourceLoader&)loader
{
    return *_loader;
}

@dynamic didPassCORSAccessChecks;
- (BOOL)didPassCORSAccessChecks
{
    return self.corsResults == WebCoreNSURLSessionCORSAccessCheckResults::Pass;
}

- (BOOL)isCrossOrigin:(const WebCore::SecurityOrigin &)origin
{
    ASSERT(isMainThread());
    Locker<Lock> locker(_dataTasksLock);
    for (auto& responseOrigin : _origins) {
        if (!origin.isSameOriginDomain(*responseOrigin))
            return true;
    }
    return false;
}

- (void)finishTasksAndInvalidate
{
    self.invalidated = YES;
    {
        Locker<Lock> locker(_dataTasksLock);
        if (!_dataTasks.isEmpty())
            return;
    }

    [self addDelegateOperation:[strongSelf = retainPtr(self)] {
        RetainPtr<id<NSURLSessionDelegate> _Nullable> delegate = strongSelf.get().delegate;
        if ([delegate respondsToSelector:@selector(URLSession:didBecomeInvalidWithError:)])
            [delegate URLSession:(NSURLSession *)strongSelf.get() didBecomeInvalidWithError:nil];
    }];
}

- (void)invalidateAndCancel
{
    Vector<RetainPtr<WebCoreNSURLSessionDataTask>> tasksCopy;
    {
        Locker<Lock> locker(_dataTasksLock);
        tasksCopy = copyToVector(_dataTasks);
    }

    for (auto& task : tasksCopy)
        [task cancel];

    [self finishTasksAndInvalidate];
}

- (void)resetWithCompletionHandler:(void (^)(void))completionHandler
{
    // FIXME: This cannot currently be implemented. We cannot guarantee that the next connection will happen on a new socket.
    [self addDelegateOperation:[completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    }];
}

- (void)flushWithCompletionHandler:(void (^)(void))completionHandler
{
    // FIXME: This cannot currently be implemented. We cannot guarantee that the next connection will happen on a new socket.
    [self addDelegateOperation:[completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    }];
}

- (void)getTasksWithCompletionHandler:(void (^)(NSArray<NSURLSessionDataTask *> *dataTasks, NSArray<NSURLSessionUploadTask *> *uploadTasks, NSArray<NSURLSessionDownloadTask *> *downloadTasks))completionHandler
{
    RetainPtr<NSArray> array;
    {
        Locker<Lock> locker(_dataTasksLock);
        array = createNSArray(_dataTasks, [] (auto& task) {
            return task.get();
        });
    }
    [self addDelegateOperation:[array, completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler(array.get(), nil, nil);
    }];
}

- (void)getAllTasksWithCompletionHandler:(void (^)(NSArray<__kindof NSURLSessionTask *> *tasks))completionHandler
{
    RetainPtr<NSArray> array;
    {
        Locker<Lock> locker(_dataTasksLock);
        array = createNSArray(_dataTasks, [] (auto& task) {
            return task.get();
        });
    }
    [self addDelegateOperation:[array, completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler(array.get());
    }];
}

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request
{
    if (self.invalidated)
        return nil;

    auto task = adoptNS([[WebCoreNSURLSessionDataTask alloc] initWithSession:self identifier:++_nextTaskIdentifier request:request targetDispatcher:_targetDispatcher.get()]);
    {
        Locker<Lock> locker(_dataTasksLock);
        _dataTasks.add(task.get());
    }
    return (NSURLSessionDataTask *)task.autorelease();
}

- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url
{
    return [self dataTaskWithRequest:[NSURLRequest requestWithURL:url]];
}

- (void)sendH2Ping:(NSURL *)url pongHandler:(void (^)(NSError *error, NSTimeInterval interval))pongHandler
{
    _targetDispatcher->dispatch([protectedSelf = retainPtr(self), self, url = retainPtr(url), pongHandler = makeBlockPtr(pongHandler)] () mutable {
        if (self.invalidated)
            return pongHandler(adoptNS([[NSError alloc] initWithDomain:NSURLErrorDomain code:NSURLErrorUnknown userInfo:nil]).get(), 0);

        Ref { self.loader }->sendH2Ping(url.get(), [self, protectedSelf = WTF::move(protectedSelf), pongHandler = WTF::move(pongHandler)] (Expected<Seconds, ResourceError>&& result) mutable {
            NSTimeInterval interval = 0;
            RetainPtr<NSError> error;
            if (result)
                interval = result.value().value();
            else
                error = result.error();
            [self addDelegateOperation:[pongHandler = WTF::move(pongHandler), error = WTF::move(error), interval] () mutable {
                pongHandler(error.get(), interval);
            }];
        });
    });
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(fileURL);
    return nil;
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(bodyData);
    return nil;
}

- (NSURLSessionUploadTask *)uploadTaskWithStreamedRequest:(NSURLRequest *)request
{
    UNUSED_PARAM(request);
    return nil;
}

- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request
{
    UNUSED_PARAM(request);
    return nil;
}

- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url
{
    UNUSED_PARAM(url);
    return nil;
}

- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData
{
    UNUSED_PARAM(resumeData);
    return nil;
}

- (NSURLSessionStreamTask *)streamTaskWithHostName:(NSString *)hostname port:(NSInteger)port
{
    UNUSED_PARAM(hostname);
    UNUSED_PARAM(port);
    return nil;
}

- (NSURLSessionStreamTask *)streamTaskWithNetService:(NSNetService *)service
{
    UNUSED_PARAM(service);
    return nil;
}

- (BOOL)isKindOfClass:(Class)aClass
{
    if (aClass == [NSURLSession class])
        return YES;
    return [super isKindOfClass:aClass];
}
@end

#pragma mark - WebCoreNSURLSessionDataTaskClient

namespace WebCore {

class WebCoreNSURLSessionDataTaskClient : public PlatformMediaResourceClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebCoreNSURLSessionDataTaskClient);
public:
    WebCoreNSURLSessionDataTaskClient(WebCoreNSURLSessionDataTask *task, GuaranteedSerialFunctionDispatcher& target)
        : m_task(task)
        , m_targetDispatcher(target)
    {
    }

    void clearTask();

    void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(ShouldContinuePolicyCheck)>&&) override;
    void redirectReceived(PlatformMediaResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&) override;
    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) override;
    void dataSent(PlatformMediaResource&, unsigned long long, unsigned long long) override;
    void dataReceived(PlatformMediaResource&, const SharedBuffer&) override;
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) override;
    void loadFailed(PlatformMediaResource&, const ResourceError&) override;
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) override;

private:
    bool isWebCoreNSURLSessionDataTaskClient() const final { return true; }

    WeakObjCPtr<WebCoreNSURLSessionDataTask> m_task WTF_GUARDED_BY_CAPABILITY(m_targetDispatcher.get());
    const Ref<GuaranteedSerialFunctionDispatcher> m_targetDispatcher;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebCoreNSURLSessionDataTaskClient) \
    static bool isType(const WebCore::PlatformMediaResourceClient& client) { return client.isWebCoreNSURLSessionDataTaskClient(); } \
SPECIALIZE_TYPE_TRAITS_END()

namespace WebCore {

void WebCoreNSURLSessionDataTaskClient::clearTask()
{
    assertIsCurrent(m_targetDispatcher.get());
    m_task = nullptr;
}

void WebCoreNSURLSessionDataTaskClient::dataSent(PlatformMediaResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resource:&resource sentBytes:bytesSent totalBytesToBeSent:totalBytesToBeSent];
}

void WebCoreNSURLSessionDataTaskClient::responseReceived(PlatformMediaResource& resource, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    assertIsCurrent(m_targetDispatcher.get());
    Ref protectedThis { *this };
    RetainPtr task = m_task.get();
    if (!task)
        return completionHandler(ShouldContinuePolicyCheck::No);

    [task resource:&resource receivedResponse:response completionHandler:WTF::move(completionHandler)];
}

bool WebCoreNSURLSessionDataTaskClient::shouldCacheResponse(PlatformMediaResource& resource, const ResourceResponse& response)
{
    assertIsCurrent(m_targetDispatcher.get());
    RetainPtr task = m_task.get();
    if (!task)
        return false;

    return [task resource:&resource shouldCacheResponse:response];
}

void WebCoreNSURLSessionDataTaskClient::dataReceived(PlatformMediaResource& resource, const SharedBuffer& buffer)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resource:&resource receivedData:buffer.createNSData()];
}

void WebCoreNSURLSessionDataTaskClient::redirectReceived(PlatformMediaResource& resource, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resource:&resource receivedRedirect:response request:WTF::move(request) completionHandler:WTF::move(completionHandler)];
}

void WebCoreNSURLSessionDataTaskClient::accessControlCheckFailed(PlatformMediaResource& resource, const ResourceError& error)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resource:&resource accessControlCheckFailedWithError:error];
}

void WebCoreNSURLSessionDataTaskClient::loadFailed(PlatformMediaResource& resource, const ResourceError& error)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resource:&resource loadFailedWithError:error];
}

void WebCoreNSURLSessionDataTaskClient::loadFinished(PlatformMediaResource& resource, const NetworkLoadMetrics& metrics)
{
    assertIsCurrent(m_targetDispatcher.get());
    if (RetainPtr task = m_task.get())
        [task resourceFinished:&resource metrics:metrics];
}

}

#pragma mark - WebCoreNSURLSessionDataTask

@implementation WebCoreNSURLSessionDataTask
- (id)initWithSession:(WebCoreNSURLSession *)session identifier:(NSUInteger)identifier request:(NSURLRequest *)request targetDispatcher:(GuaranteedSerialFunctionDispatcher *)dispatcher
{
    self.taskIdentifier = identifier;
    self->_state = NSURLSessionTaskStateSuspended;
    self.priority = NSURLSessionTaskPriorityDefault;
    self.session = session;
    _targetDispatcher = dispatcher;
    _resumeSessionID = 0;

    // CoreMedia will explicitly add a user agent header. Remove if present.
    if ([request valueForHTTPHeaderField:@"User-Agent"]) {
        RetainPtr mutableRequest = adoptNS([request mutableCopy]);
        [mutableRequest setValue:nil forHTTPHeaderField:@"User-Agent"];
        self->_originalRequest = self->_currentRequest = mutableRequest;
    } else
        self->_originalRequest = self->_currentRequest = request;

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    UNUSED_PARAM(zone);
    return [self retain];
}

#pragma mark - Internal methods

- (void)_cancel
{
    assertIsCurrent(*_targetDispatcher);

    RefPtr<WebCore::PlatformMediaResource> resource = self.resource;
    if (resource)
        resource->shutdown();
    RetainPtr<WebCoreNSURLSession> strongSession = self.session;
    if (strongSession)
        Ref { [strongSession rangeResponseGenerator] }->removeTask(self);
    self.resource = nullptr;
}

#pragma mark - NSURLSession API
@synthesize taskIdentifier = _taskIdentifier;
@synthesize countOfBytesReceived = _countOfBytesReceived;
@synthesize countOfBytesSent = _countOfBytesSent;
@synthesize countOfBytesExpectedToSend = _countOfBytesExpectedToSend;
@synthesize countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
@synthesize priority = _priority;

- (NSURLRequest *)originalRequest
{
    return adoptNS([_originalRequest copy]).autorelease();
}

- (NSURLRequest *)currentRequest
{
    return adoptNS([_currentRequest copy]).autorelease();
}

- (NSError *)error
{
    // TODO: _error is never set and is always nil.
    return adoptNS([_error copy]).autorelease();
}

- (NSString *)taskDescription
{
    return adoptNS([_taskDescription copy]).autorelease();
}

- (void)setTaskDescription:(NSString *)description
{
    _taskDescription = adoptNS([description copy]);
}

- (WebCoreNSURLSession *)session
{
    @synchronized(self) {
        return _session.get().autorelease();
    }
}

- (void)setSession:(WebCoreNSURLSession *)session
{
    @synchronized(self) {
        _session = session;
    }
}

- (WebCore::PlatformMediaResource *)resource
{
    assertIsCurrent(*_targetDispatcher);

    return _resource.get();
}

- (void)setResource:(WebCore::PlatformMediaResource *)resource
{
    assertIsCurrent(*_targetDispatcher);

    _resource = resource;
}

- (NSURLResponse *)response
{
    return _response.get();
}

- (NSURLSessionTaskState)state
{
    return _state;
}

- (void)cancel
{
    _targetDispatcher->dispatch([protectedSelf = retainPtr(self), self] () mutable {
        if (self.state == NSURLSessionTaskStateCompleted)
            return;
        self->_state = NSURLSessionTaskStateCanceling;
        [self _cancel];
        [self _resource:nullptr loadFinishedWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled userInfo:nil] metrics:NetworkLoadMetrics { }];
    });
}

- (void)suspend
{
    _targetDispatcher->dispatch([protectedSelf = retainPtr(self), self] {
        if (self.state != NSURLSessionTaskStateRunning)
            return;
        self->_state = NSURLSessionTaskStateSuspended;
        // NSURLSessionDataTasks must start over after suspending, so while
        // we could defer loading at this point, instead cancel and restart
        // upon resume so as to adhere to NSURLSessionDataTask semantics.
        [self _cancel];
    });
}

- (void)resume
{
    _targetDispatcher->dispatch([protectedSelf = retainPtr(self), self] () mutable {
        if (self.state != NSURLSessionTaskStateSuspended)
            return;
        [self _cancel];
        RetainPtr<WebCoreNSURLSession> strongSession = self.session;
        if (self.state != NSURLSessionTaskStateSuspended || !strongSession)
            return;
        self->_state = NSURLSessionTaskStateRunning;
        if (Ref { [strongSession rangeResponseGenerator] }->willHandleRequest(self, retainPtr(self.originalRequest).get()))
            return;
        _resumeSessionID++;
        Ref loader = [strongSession loader];
        if (auto resource = loader->requestResource(self.originalRequest, PlatformMediaResourceLoader::LoadOption::DisallowCaching)) {
            resource->setClient(adoptRef(*new WebCoreNSURLSessionDataTaskClient(protectedSelf.get(), *_targetDispatcher)));
            if (self->_state != NSURLSessionTaskStateRunning) {
                resource->shutdown();
                return;
            }
            self.resource = resource.get();
            return;
        }
        // A nil return from requestResource means the load was cancelled by a delegate client
        [self _resource:nil loadFinishedWithError:ResourceError(ResourceError::Type::Cancellation).protectedNSError().get() metrics: { }];
    });
}

- (void)dealloc
{
    if (RefPtr<PlatformMediaResource> resource = std::exchange(_resource, { })) {
        _targetDispatcher->dispatch([resource = WTF::move(resource)] () mutable {
            if (RefPtr client = resource->client())
                downcast<WebCoreNSURLSessionDataTaskClient>(*client).clearTask();
            resource->shutdown();
        });
    }

    [super dealloc];
}

#pragma mark - NSURLSession SPI

- (NSDictionary *)_timingData
{
    // FIXME: Make sure nobody is using this and remove this. It is replaced by WebCoreNSURLSessionTaskTransactionMetrics.
    return @{ };
}

#pragma mark - PlatformMediaResourceClient callbacks

- (void)resource:(PlatformMediaResource*)resource sentBytes:(unsigned long long)bytesSent totalBytesToBeSent:(unsigned long long)totalBytesToBeSent
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);
    UNUSED_PARAM(bytesSent);
    UNUSED_PARAM(totalBytesToBeSent);
    // No-op.
}

- (void)resource:(PlatformMediaResource*)resource receivedResponse:(const ResourceResponse&)response completionHandler:(CompletionHandler<void(ShouldContinuePolicyCheck)>&&)completionHandler
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);

    RetainPtr<WebCoreNSURLSession> strongSession { self.session };
    [strongSession task:self addSecurityOrigin:SecurityOrigin::create(response.url())];
    [strongSession task:self didReceiveCORSAccessCheckResult:resource ? resource->didPassAccessControlCheck() : YES];
    self.countOfBytesExpectedToReceive = response.expectedContentLength();
    RetainPtr<NSURLResponse> strongResponse = response.nsURLResponse();

    if (resource && strongSession && Ref { [strongSession rangeResponseGenerator] }->willSynthesizeRangeResponses(self, *resource, response)) {
        // The RangeResponseGenerator took a strong reference to resource. We can reset it now.
        self.resource = nullptr;
        return completionHandler(ShouldContinuePolicyCheck::Yes);
    }

    RetainPtr<WebCoreNSURLSessionDataTask> strongSelf { self };
    if (!strongSession)
        return completionHandler(ShouldContinuePolicyCheck::No);
    [strongSession addDelegateOperation:[strongSelf, strongResponse, completionHandler = WTF::move(completionHandler), targetDispatcher = _targetDispatcher] () mutable {
        strongSelf->_response = strongResponse.get();

        RetainPtr<id<NSURLSessionDataDelegate>> dataDelegate = (id<NSURLSessionDataDelegate>)strongSelf.get().session.delegate;
        if (![dataDelegate respondsToSelector:@selector(URLSession:dataTask:didReceiveResponse:completionHandler:)]) {
            targetDispatcher->dispatch([strongSelf, completionHandler = WTF::move(completionHandler)] () mutable {
                completionHandler(ShouldContinuePolicyCheck::Yes);
            });
            return;
        }

        [dataDelegate URLSession:(NSURLSession *)retainPtr(strongSelf.get().session).get() dataTask:(NSURLSessionDataTask *)strongSelf.get() didReceiveResponse:strongResponse.get() completionHandler:makeBlockPtr([strongSelf, targetDispatcher = WTF::move(targetDispatcher), completionHandler = WTF::move(completionHandler)] (NSURLSessionResponseDisposition disposition) mutable {
            targetDispatcher->dispatch([strongSelf, disposition, completionHandler = WTF::move(completionHandler)] () mutable {
                if (disposition == NSURLSessionResponseCancel)
                    completionHandler(ShouldContinuePolicyCheck::No);
                else {
                    ASSERT(disposition == NSURLSessionResponseAllow);
                    completionHandler(ShouldContinuePolicyCheck::Yes);
                }
            });
        }).get()];
    }];
}

- (BOOL)resource:(PlatformMediaResource*)resource shouldCacheResponse:(const ResourceResponse&)response
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);

    // FIXME: remove if <rdar://problem/20001985> is ever resolved.
    return response.httpHeaderField(HTTPHeaderName::ContentRange).isEmpty();
}

- (void)resource:(PlatformMediaResource*)resource receivedData:(RetainPtr<NSData>&&)data
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);
    RetainPtr<WebCoreNSURLSession> strongSession { self.session };
    [strongSession addDelegateOperation:[strongSelf = RetainPtr { self }, data = WTF::move(data)] {
        strongSelf.get().countOfBytesReceived += [data length];
        RetainPtr<id<NSURLSessionDataDelegate>> dataDelegate = (id<NSURLSessionDataDelegate>)strongSelf.get().session.delegate;
        if ([dataDelegate respondsToSelector:@selector(URLSession:dataTask:didReceiveData:)])
            [dataDelegate URLSession:(NSURLSession *)retainPtr(strongSelf.get().session).get() dataTask:(NSURLSessionDataTask *)strongSelf.get() didReceiveData:data.get()];
    }];
}

- (void)resource:(PlatformMediaResource*)resource receivedRedirect:(const ResourceResponse&)response request:(ResourceRequest&&)request completionHandler:(CompletionHandler<void(ResourceRequest&&)>&&)completionHandler
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);
    RetainPtr<WebCoreNSURLSession> strongSession { self.session };
    [strongSession task:self addSecurityOrigin:SecurityOrigin::create(response.url())];
    [strongSession addDelegateOperation:[strongSelf = retainPtr(self), response = response.protectedNSURLResponse(), request = request.isolatedCopy(), completionHandler = WTF::move(completionHandler), targetDispatcher = _targetDispatcher] () mutable {
        if (![response isKindOfClass:[NSHTTPURLResponse class]]) {
            ASSERT_NOT_REACHED();
            targetDispatcher->dispatch([request = WTF::move(request), completionHandler = WTF::move(completionHandler)] () mutable {
                completionHandler(WTF::move(request));
            });
            return;
        }

        RetainPtr<id<NSURLSessionDataDelegate>> dataDelegate = (id<NSURLSessionDataDelegate>)strongSelf.get().session.delegate;
        if ([dataDelegate respondsToSelector:@selector(URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:)]) {
            auto completionHandlerBlock = makeBlockPtr([completionHandler = WTF::move(completionHandler), targetDispatcher = WTF::move(targetDispatcher)](NSURLRequest *newRequest) mutable {
                targetDispatcher->dispatch([request = ResourceRequest { newRequest }, completionHandler = WTF::move(completionHandler)] () mutable {
                    completionHandler(WTF::move(request));
                });
            });
            [dataDelegate URLSession:(NSURLSession *)retainPtr(strongSelf.get().session).get() task:(NSURLSessionTask *)strongSelf.get() willPerformHTTPRedirection:(NSHTTPURLResponse *)response.get() newRequest:request.protectedNSURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).get() completionHandler:completionHandlerBlock.get()];
        } else {
            targetDispatcher->dispatch([request = WTF::move(request), completionHandler = WTF::move(completionHandler)] () mutable {
                completionHandler(WTF::move(request));
            });
        }
    }];
}

- (void)_resource:(PlatformMediaResource*)resource loadFinishedWithError:(NSError *)error metrics:(const NetworkLoadMetrics&)metrics
{
    assertIsCurrent(*_targetDispatcher);
    ASSERT_UNUSED(resource, !resource || resource == self.resource || !self.resource);
    if (self.state == NSURLSessionTaskStateCompleted)
        return;
    self->_state = NSURLSessionTaskStateCompleted;

    RetainPtr<WebCoreNSURLSessionDataTask> strongSelf { self };
    RetainPtr<WebCoreNSURLSession> strongSession { self.session };
    RetainPtr<NSError> strongError { error };
    auto taskMetrics = adoptNS([[WebCoreNSURLSessionTaskMetrics alloc] _initWithMetrics:metrics.isolatedCopy() onTarget:_targetDispatcher.get()]);
    [strongSession addDelegateOperation:[strongSelf, strongSession, strongError, taskMetrics = WTF::move(taskMetrics), targetDispatcher = _targetDispatcher] () mutable {
        RetainPtr<id<NSURLSessionTaskDelegate>> delegate = (id<NSURLSessionTaskDelegate>)strongSession.get().delegate;

        if ([delegate respondsToSelector:@selector(URLSession:task:didFinishCollectingMetrics:)])
            [delegate URLSession:(NSURLSession *)strongSession.get() task:(NSURLSessionDataTask *)strongSelf.get() didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)taskMetrics.get()];

        if ([delegate respondsToSelector:@selector(URLSession:task:didCompleteWithError:)])
            [delegate URLSession:(NSURLSession *)strongSession.get() task:(NSURLSessionDataTask *)strongSelf.get() didCompleteWithError:strongError.get()];

        targetDispatcher->dispatch([strongSelf, strongSession] {
            [strongSession taskCompleted:strongSelf.get()];
        });
    }];
}

- (void)resource:(PlatformMediaResource*)resource accessControlCheckFailedWithError:(const ResourceError&)error
{
    assertIsCurrent(*_targetDispatcher);
    [self _resource:resource loadFinishedWithError:error.protectedNSError().get() metrics:NetworkLoadMetrics { }];
}

- (void)resource:(PlatformMediaResource*)resource loadFailedWithError:(const ResourceError&)error
{
    assertIsCurrent(*_targetDispatcher);
    [self _resource:resource loadFinishedWithError:error.protectedNSError().get() metrics:NetworkLoadMetrics { }];
}

- (void)resourceFinished:(PlatformMediaResource*)resource metrics:(const NetworkLoadMetrics&)metrics
{
    assertIsCurrent(*_targetDispatcher);
    [self _resource:resource loadFinishedWithError:nil metrics:metrics];
}
@end

/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"

#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestScriptMessageHandler.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

static void keepNetworkProcessActive()
{
    [WKWebsiteDataStore.defaultDataStore fetchDataRecordsOfTypes:WKWebsiteDataStore.allWebsiteDataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> *) {
        keepNetworkProcessActive();
    }];
}

TEST(IndexedDB, IndexedDBSuspendImminently)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr handler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBSuspendImminently" withExtension:@"html"]];
    [webView loadRequest:request];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"Continue");

    readyToContinue = false;
    __block bool suspended = false;
    [configuration.get().websiteDataStore _sendNetworkProcessWillSuspendImminently];

    // Ensure transaction is aborted before sending resume message.
    while (!suspended) {
        [configuration.get().websiteDataStore _isStorageSuspendedForTesting:^(BOOL isSuspended) {
            suspended = isSuspended;
            readyToContinue = true;
        }];
        TestWebKitAPI::Util::run(&readyToContinue);
        readyToContinue = false;
    }

    [configuration.get().websiteDataStore _sendNetworkProcessDidResume];
    keepNetworkProcessActive();

    EXPECT_WK_STREQ([handler waitForMessage].body, @"Expected Abort For Suspension");
    EXPECT_WK_STREQ([handler waitForMessage].body, @"Expected Success After Resume");
}

TEST(IndexedDB, SuspendImminentlyForThirdPartyDatabases)
{
    static NSString *mainFrameString = @"<script> \
        function postResult(event) { \
            window.webkit.messageHandlers.testHandler.postMessage(event.data); \
        } \
        addEventListener('message', postResult, false); \
        </script> \
        <iframe src='iframe://'>";

    static const char* iframeBytes = R"TESTRESOURCE(
    <script>
    function postResult(result) {
        if (window.parent != window.top) {
            parent.postMessage(result, '*');
        } else {
            window.webkit.messageHandlers.testHandler.postMessage(result);
        }
    }

    try {
        var request = window.indexedDB.open('IndexedDBSuspendImminentlyForThirdPartyDatabases');
        request.onupgradeneeded = function(event) {
            var db = event.target.result;
            var os = db.createObjectStore('TestObjectStore');
            var transaction = event.target.transaction;
            transaction.onabort = function(event) {
                postResult('transaction is aborted');
            }
            transaction.oncomplete = function(event) {
                postResult('transaction is completed');
            }

            postResult('database is created');

            for (let i = 0; i < 1000; i ++)
                os.put('TestValue', 'TestKey');
        }
        request.onerror = function(event) {
            postResult('database error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    } catch(err) {
        postResult('database error: ' + err.name + ' - ' + err.message);
    }
    </script>
    )TESTRESOURCE";

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr handler = adoptNS([TestScriptMessageHandler new]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:iframeBytes length:strlen(iframeBytes)]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"iframe"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"database is created");

    [configuration.get().websiteDataStore _sendNetworkProcessWillSuspendImminently];
    [configuration.get().websiteDataStore _sendNetworkProcessDidResume];

    EXPECT_WK_STREQ([handler waitForMessage].body, @"transaction is completed");
}

#endif // PLATFORM(IOS_FAMILY)

TEST(IndexedDB, TransactionOfSuspendedProcessDoesNotBlockOtherProcesses)
{
    static NSString *firstClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('SuspendedProcessContentionDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('TestObjectStore'); \
        }; \
        request.onerror = function() { \
            post('first database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('TestObjectStore', 'readwrite'); \
            transaction.onabort = function() { \
                post('first transaction aborted'); \
            }; \
            transaction.oncomplete = function() { \
                post('first transaction completed'); \
            }; \
            var objectStore = transaction.objectStore('TestObjectStore'); \
            var started = false; \
            function keepTransactionAlive() { \
                var putRequest = objectStore.put('TestValue', 'TestKey'); \
                putRequest.onsuccess = function() { \
                    if (!started) { \
                        started = true; \
                        post('first transaction started'); \
                    } \
                    keepTransactionAlive(); \
                }; \
            } \
            keepTransactionAlive(); \
        }; \
        </script>";

    static NSString *secondClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('SuspendedProcessContentionDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('TestObjectStore'); \
        }; \
        request.onerror = function() { \
            post('second database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('TestObjectStore', 'readwrite'); \
            transaction.onabort = function() { \
                post('second transaction aborted'); \
            }; \
            transaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            transaction.objectStore('TestObjectStore').put('OtherValue', 'OtherKey'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    // Two web views in different process pools so they run in different WebContent
    // processes, sharing one data store so they use the same network process.
    RetainPtr firstHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr firstConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [firstConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[firstConfiguration userContentController] addScriptMessageHandler:firstHandler.get() name:@"testHandler"];
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:firstConfiguration.get()]);
    [firstWebView loadHTMLString:firstClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction started");

    // Mark the first web view's process as suspended; its read-write transaction can
    // no longer finish on its own.
    [firstWebView _setThrottleStateForTesting:0];

    RetainPtr secondHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr secondConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [secondConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[secondConfiguration userContentController] addScriptMessageHandler:secondHandler.get() name:@"testHandler"];
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:secondConfiguration.get()]);
    [secondWebView loadHTMLString:secondClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    // Without aborting the suspended process's in-progress transaction, this transaction
    // would be queued behind it forever.
    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second transaction completed");

    // The first client learns about the abort the next time it uses the transaction.
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction aborted");
}

TEST(IndexedDB, ReadOnlyTransactionOfSuspendedProcessDoesNotBlockOtherProcesses)
{
    static NSString *firstClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('ReadOnlyContentionDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('TestObjectStore'); \
        }; \
        request.onerror = function() { \
            post('first database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('TestObjectStore', 'readonly'); \
            transaction.onabort = function() { \
                post('first transaction aborted'); \
            }; \
            var objectStore = transaction.objectStore('TestObjectStore'); \
            var started = false; \
            function keepTransactionAlive() { \
                var getRequest = objectStore.get('TestKey'); \
                getRequest.onsuccess = function() { \
                    if (!started) { \
                        started = true; \
                        post('first transaction started'); \
                    } \
                    keepTransactionAlive(); \
                }; \
            } \
            keepTransactionAlive(); \
        }; \
        </script>";

    static NSString *secondClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('ReadOnlyContentionDatabase'); \
        request.onerror = function() { \
            post('second database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('TestObjectStore', 'readwrite'); \
            transaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            transaction.objectStore('TestObjectStore').put('OtherValue', 'OtherKey'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr firstHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr firstConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [firstConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[firstConfiguration userContentController] addScriptMessageHandler:firstHandler.get() name:@"testHandler"];
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:firstConfiguration.get()]);
    [firstWebView loadHTMLString:firstClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction started");

    // The in-progress read-only transaction belongs to the suspended process and shares an object
    // store with the second process's read-write transaction, so it blocks it through the
    // scope-overlap path (rather than the read-write serialization path).
    [firstWebView _setThrottleStateForTesting:0];

    RetainPtr secondHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr secondConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [secondConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[secondConfiguration userContentController] addScriptMessageHandler:secondHandler.get() name:@"testHandler"];
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:secondConfiguration.get()]);
    [secondWebView loadHTMLString:secondClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    // The read-write transaction completes only if the suspended process's overlapping read-only
    // transaction is aborted.
    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second transaction completed");
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction aborted");
}

TEST(IndexedDB, UncontendedTransactionOfSuspendedProcessIsNotAborted)
{
    static NSString *firstClientString = @"<script> \
        var stopTransaction = false; \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('UncontendedSuspensionDatabase'); \
        request.onerror = function() { \
            post('first database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('FirstObjectStore', 'readonly'); \
            transaction.onabort = function() { \
                post('first transaction aborted'); \
            }; \
            transaction.oncomplete = function() { \
                post('first transaction completed'); \
            }; \
            var objectStore = transaction.objectStore('FirstObjectStore'); \
            var started = false; \
            function keepTransactionAlive() { \
                if (stopTransaction) \
                    return; \
                var getRequest = objectStore.get('TestKey'); \
                getRequest.onsuccess = function() { \
                    if (!started) { \
                        started = true; \
                        post('first transaction started'); \
                    } \
                    keepTransactionAlive(); \
                }; \
            } \
            keepTransactionAlive(); \
        }; \
        </script>";

    static NSString *secondClientString = @"<script> \
        var database = null; \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        function runTransaction() { \
            var transaction = database.transaction('SecondObjectStore', 'readwrite'); \
            transaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            transaction.objectStore('SecondObjectStore').put('OtherValue', 'OtherKey'); \
        } \
        var request = indexedDB.open('UncontendedSuspensionDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('FirstObjectStore'); \
            event.target.result.createObjectStore('SecondObjectStore'); \
        }; \
        request.onerror = function() { \
            post('second database error'); \
        }; \
        request.onsuccess = function(event) { \
            database = event.target.result; \
            post('second ready'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    // Bring up the second web view first and create the database. Keeping its process unsuspended
    // for the rest of the test keeps the network process out of suspension; otherwise, once the
    // first process is suspended, the network process could suspend too and abort every in-progress
    // transaction through its own suspension path, masking whether the new contention-based abort
    // left the uncontended transaction alone.
    RetainPtr secondHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr secondConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [secondConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[secondConfiguration userContentController] addScriptMessageHandler:secondHandler.get() name:@"testHandler"];
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:secondConfiguration.get()]);
    [secondWebView loadHTMLString:secondClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second ready");

    // Start the first process's read-only transaction on a different object store.
    RetainPtr firstHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr firstConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [firstConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[firstConfiguration userContentController] addScriptMessageHandler:firstHandler.get() name:@"testHandler"];
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:firstConfiguration.get()]);
    [firstWebView loadHTMLString:firstClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction started");

    // Suspend the first process. Its in-progress read-only transaction touches a different object
    // store than the second process's read-write transaction, so it doesn't block it and must be
    // left alone.
    [firstWebView _setThrottleStateForTesting:0];

    // The second transaction runs concurrently because it doesn't overlap the suspended one.
    [secondWebView evaluateJavaScript:@"runTransaction();" completionHandler:nil];
    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second transaction completed");

    // Resume the first process and let its transaction finish. It should complete rather than
    // abort, confirming the uncontended transaction was never touched.
    [firstWebView _setThrottleStateForTesting:2];
    [firstWebView evaluateJavaScript:@"stopTransaction = true;" completionHandler:nil];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction completed");
}

TEST(IndexedDB, AbortMultipleTransactionsOfSuspendedProcess)
{
    static NSString *firstClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var storeCount = 10; \
        var startedCount = 0; \
        var abortedCount = 0; \
        var request = indexedDB.open('ManySuspendedTransactionsDatabase'); \
        request.onupgradeneeded = function(event) { \
            var db = event.target.result; \
            for (var i = 0; i < storeCount; i++) \
                db.createObjectStore('Store' + i); \
        }; \
        request.onerror = function() { \
            post('first database error'); \
        }; \
        request.onsuccess = function(event) { \
            var db = event.target.result; \
            for (var i = 0; i < storeCount; i++) { \
                (function(storeName) { \
                    var transaction = db.transaction(storeName, 'readonly'); \
                    transaction.onabort = function() { \
                        abortedCount++; \
                        if (abortedCount === storeCount) \
                            post('all first transactions aborted'); \
                    }; \
                    var objectStore = transaction.objectStore(storeName); \
                    var started = false; \
                    function keepTransactionAlive() { \
                        var getRequest = objectStore.get('TestKey'); \
                        getRequest.onsuccess = function() { \
                            if (!started) { \
                                started = true; \
                                if (++startedCount === storeCount) \
                                    post('first transactions started'); \
                            } \
                            keepTransactionAlive(); \
                        }; \
                    } \
                    keepTransactionAlive(); \
                })('Store' + i); \
            } \
        }; \
        </script>";

    static NSString *secondClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var storeCount = 10; \
        var storeNames = []; \
        for (var i = 0; i < storeCount; i++) \
            storeNames.push('Store' + i); \
        var request = indexedDB.open('ManySuspendedTransactionsDatabase'); \
        request.onerror = function() { \
            post('second database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction(storeNames, 'readwrite'); \
            transaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            transaction.objectStore(storeNames[0]).put('OtherValue', 'OtherKey'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr firstHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr firstConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [firstConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[firstConfiguration userContentController] addScriptMessageHandler:firstHandler.get() name:@"testHandler"];
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:firstConfiguration.get()]);
    [firstWebView loadHTMLString:firstClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transactions started");

    [firstWebView _setThrottleStateForTesting:0];

    RetainPtr secondHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr secondConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [secondConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[secondConfiguration userContentController] addScriptMessageHandler:secondHandler.get() name:@"testHandler"];
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:secondConfiguration.get()]);
    [secondWebView loadHTMLString:secondClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second transaction completed");
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"all first transactions aborted");
}

TEST(IndexedDB, ManyQueuedTransactionsOfSuspendedProcessAreDeferredNotAborted)
{
    static NSString *firstClientString = @"<script> \
        var transactionCount = 100; \
        var completedCount = 0; \
        var abortedCount = 0; \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        function checkQueuedTransactionsSettled() { \
            if (completedCount + abortedCount === transactionCount - 1) \
                post('queued transactions settled completed=' + completedCount + ' aborted=' + abortedCount); \
        } \
        var request = indexedDB.open('ManyQueuedSuspendedTransactionsDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('Store'); \
        }; \
        request.onerror = function() { \
            post('first database error'); \
        }; \
        request.onsuccess = function(event) { \
            var db = event.target.result; \
            var firstTransaction = db.transaction('Store', 'readwrite'); \
            firstTransaction.onabort = function() { \
                post('first transaction aborted'); \
            }; \
            var objectStore = firstTransaction.objectStore('Store'); \
            function keepTransactionAlive() { \
                objectStore.get('TestKey').onsuccess = keepTransactionAlive; \
            } \
            keepTransactionAlive(); \
            for (var i = 1; i < transactionCount; i++) { \
                var transaction = db.transaction('Store', 'readwrite'); \
                transaction.onabort = function() { \
                    abortedCount++; \
                    checkQueuedTransactionsSettled(); \
                }; \
                transaction.oncomplete = function() { \
                    completedCount++; \
                    checkQueuedTransactionsSettled(); \
                }; \
                transaction.objectStore('Store').put('Value' + i, 'Key' + i); \
            } \
            objectStore.get('TestKey').onsuccess = function() { \
                post('first transactions queued'); \
            }; \
        }; \
        </script>";

    static NSString *secondClientString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('ManyQueuedSuspendedTransactionsDatabase'); \
        request.onerror = function() { \
            post('second database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('Store', 'readwrite'); \
            transaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            transaction.objectStore('Store').put('OtherValue', 'OtherKey'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr firstHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr firstConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [firstConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[firstConfiguration userContentController] addScriptMessageHandler:firstHandler.get() name:@"testHandler"];
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:firstConfiguration.get()]);
    [firstWebView loadHTMLString:firstClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    // This request is issued after every transaction, so its reply means the server has them all.
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transactions queued");
    pid_t networkProcessIdentifier = [[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier];
    EXPECT_NE(networkProcessIdentifier, 0);

    [firstWebView _setThrottleStateForTesting:0];

    RetainPtr secondHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr secondConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [secondConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[secondConfiguration userContentController] addScriptMessageHandler:secondHandler.get() name:@"testHandler"];
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:secondConfiguration.get()]);
    [secondWebView loadHTMLString:secondClientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    EXPECT_WK_STREQ([secondHandler waitForMessage].body, @"second transaction completed");
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"first transaction aborted");

    // The network process must not have crashed and relaunched.
    EXPECT_EQ(networkProcessIdentifier, [[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier]);

    [firstWebView _setThrottleStateForTesting:2];
    EXPECT_WK_STREQ([firstHandler waitForMessage].body, @"queued transactions settled completed=99 aborted=0");
}

TEST(IndexedDB, TransactionOfSuspendedProcessIsNotAbortedByItsOwnQueuedTransaction)
{
    static NSString *clientString = @"<script> \
        var stopFirstTransaction = false; \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('OwnQueuedTransactionDatabase'); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('Store'); \
        }; \
        request.onerror = function() { \
            post('database error'); \
        }; \
        request.onsuccess = function(event) { \
            var db = event.target.result; \
            var firstTransaction = db.transaction('Store', 'readwrite'); \
            firstTransaction.onabort = function() { \
                post('first transaction aborted'); \
            }; \
            firstTransaction.oncomplete = function() { \
                post('first transaction completed'); \
            }; \
            var objectStore = firstTransaction.objectStore('Store'); \
            function keepTransactionAlive() { \
                if (stopFirstTransaction) \
                    return; \
                objectStore.get('TestKey').onsuccess = keepTransactionAlive; \
            } \
            keepTransactionAlive(); \
            var secondTransaction = db.transaction('Store', 'readwrite'); \
            secondTransaction.onabort = function() { \
                post('second transaction aborted'); \
            }; \
            secondTransaction.oncomplete = function() { \
                post('second transaction completed'); \
            }; \
            secondTransaction.objectStore('Store').put('OtherValue', 'OtherKey'); \
            objectStore.get('TestKey').onsuccess = function() { \
                post('second transaction queued'); \
            }; \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr handler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:clientString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    // This request is issued after the second transaction, so its reply means the server has it.
    EXPECT_WK_STREQ([handler waitForMessage].body, @"second transaction queued");

    // The in-progress transaction only blocks a transaction of the same, suspended client, so there
    // is no live client waiting on it and it must be left alone.
    [webView _setThrottleStateForTesting:0];
    [webView _setThrottleStateForTesting:2];

    [webView evaluateJavaScript:@"stopFirstTransaction = true;" completionHandler:nil];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"first transaction completed");
    EXPECT_WK_STREQ([handler waitForMessage].body, @"second transaction completed");
}

TEST(IndexedDB, TransactionOfSuspendedProcessIsAbortedWhenAnotherSuspendedProcessResumes)
{
    static NSString *holderString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('SuspendedContentionOnResumeDatabase', 1); \
        request.onupgradeneeded = function(event) { \
            event.target.result.createObjectStore('Store'); \
            event.target.result.createObjectStore('Sync'); \
        }; \
        request.onerror = function() { \
            post('holder database error'); \
        }; \
        request.onsuccess = function(event) { \
            var transaction = event.target.result.transaction('Store', 'readwrite'); \
            transaction.onabort = function() { \
                post('holder transaction aborted'); \
            }; \
            var objectStore = transaction.objectStore('Store'); \
            var started = false; \
            function keepTransactionAlive() { \
                objectStore.put('HolderValue', 'HolderKey').onsuccess = function() { \
                    if (!started) { \
                        started = true; \
                        post('holder transaction started'); \
                    } \
                    keepTransactionAlive(); \
                }; \
            } \
            keepTransactionAlive(); \
        }; \
        </script>";

    static NSString *waiterString = @"<script> \
        function post(message) { \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        } \
        var request = indexedDB.open('SuspendedContentionOnResumeDatabase'); \
        request.onerror = function() { \
            post('waiter database error'); \
        }; \
        request.onsuccess = function(event) { \
            var db = event.target.result; \
            var blockedTransaction = db.transaction('Store', 'readwrite'); \
            blockedTransaction.oncomplete = function() { \
                post('waiter transaction completed'); \
            }; \
            blockedTransaction.objectStore('Store').put('WaiterValue', 'WaiterKey'); \
            var syncTransaction = db.transaction('Sync', 'readonly'); \
            syncTransaction.oncomplete = function() { \
                post('waiter transaction queued'); \
            }; \
            syncTransaction.objectStore('Sync').get('TestKey'); \
        }; \
        </script>";

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr holderHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr holderConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [holderConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[holderConfiguration userContentController] addScriptMessageHandler:holderHandler.get() name:@"testHandler"];
    RetainPtr holderWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:holderConfiguration.get()]);
    [holderWebView loadHTMLString:holderString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([holderHandler waitForMessage].body, @"holder transaction started");

    // The transaction on the other object store is not blocked, so its completion means the server
    // has already received the blocked one ahead of it on the same connection.
    RetainPtr waiterHandler = adoptNS([TestScriptMessageHandler new]);
    RetainPtr waiterConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [waiterConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [[waiterConfiguration userContentController] addScriptMessageHandler:waiterHandler.get() name:@"testHandler"];
    RetainPtr waiterWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:waiterConfiguration.get()]);
    [waiterWebView loadHTMLString:waiterString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([waiterHandler waitForMessage].body, @"waiter transaction queued");

    [waiterWebView _setThrottleStateForTesting:0];
    [holderWebView _setThrottleStateForTesting:0];

    [waiterWebView _setThrottleStateForTesting:2];
    EXPECT_WK_STREQ([waiterHandler waitForMessage].body, @"waiter transaction completed");
    EXPECT_WK_STREQ([holderHandler waitForMessage].body, @"holder transaction aborted");
}

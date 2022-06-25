#!/usr/bin/env perl

# Copyright (C) 2011 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# tests run-leaks using "new" leaks report version 2.0
# - The "new" 2.0 format has "leaks Report Version:  2.0" after the two header sections.

use strict;
use warnings;

use File::Spec;
use FindBin;
use lib File::Spec->catdir($FindBin::Bin, "..");
use Test::More;
use LoadAsModule qw(RunLeaks run-leaks);

my @input = split(/\n/, <<EOF);
Process:         DumpRenderTree [29903]
Path:            /Volumes/Data/Build/Debug/DumpRenderTree
Load Address:    0x102116000
Identifier:      DumpRenderTree
Version:         ??? (???)
Code Type:       X86-64 (Native)
Parent Process:  Python [29892]

Date/Time:       2011-11-14 11:12:45.706 -0800
OS Version:      Mac OS X 10.7.2 (11C74)
Report Version:  7

leaks Report Version:  2.0
leaks(12871,0xacdfa2c0) malloc: process 89617 no longer exists, stack logs deleted from /tmp/stack-logs.89617.DumpRenderTree.A2giy6.index
Process 29903: 60015 nodes malloced for 7290 KB
Process 29903: 2 leaks for 1008 total leaked bytes.
Leak: 0x7f9a3a612810  size=576  zone: DefaultMallocZone_0x10227b000   URLConnectionLoader::LoaderConnectionEventQueue  C++  CFNetwork
	0x7f3af460 0x00007fff 0x7edf2f40 0x00007fff 	`.:.....@/.~....
	0x7f3af488 0x00007fff 0xdab071b1 0x0000f068 	..:......q..h...
	0x0100000a 0x00000000 0x7edf3f50 0x00007fff 	........P?.~....
	0x00000000 0x00000000 0xdab071cc 0x0000f068 	.........q..h...
	0x01000010 0x00000000 0x3a616210 0x00007f9a 	.........ba:....
	0x00000000 0x00000000 0xdab071e5 0x0000f068 	.........q..h...
	0x00000000 0x00000000 0x00000000 0x00000000 	................
	0x00000000 0x00000000 0xdab07245 0x0000f068 	........Er..h...
	...
	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoSources0 | __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__ | MultiplexerSource::perform() | URLConnectionClient::processEvents() | URLConnectionClient::ClientConnectionEventQueue::processAllEventsAndConsumePayload(XConnectionEventInfo<XClientEvent, XClientEventParams>*, long) | URLConnectionClient::_clientWillSendRequest(_CFURLRequest const*, _CFURLResponse*, URLConnectionClient::ClientConnectionEventQueue*) | URLConnectionClient::getRequestForTransmission(unsigned char, _CFURLResponse*, _CFURLRequest const*, __CFError**) | URLConnectionLoader::pushLoaderEvent(XConnectionEventInfo<XLoaderEvent, XLoaderEventParams>*) | CFAllocatedObject::operator new(unsigned long, __CFAllocator const*) | malloc_zone_malloc 
Leak: 0x7f9a3a618090  size=432  zone: DefaultMallocZone_0x10227b000   URLConnectionInstanceData  CFType  CFNetwork
	0x7edcab28 0x00007fff 0x00012b80 0x00000001 	(..~.....+......
	0x7f3af310 0x00007fff 0x7f3af3f8 0x00007fff 	..:.......:.....
	0x4d555458 0x00000000 0x00000000 0x00002068 	XTUM........h ..
	0x00000000 0x00000000 0x00000c00 0x00000c00 	................
	0x00000000 0x00000000 0x3a6180c8 0x00007f9a 	..........a:....
	0x3a6180cc 0x00007f9a 0x00000000 0x00000000 	..a:............
	0x7f3af418 0x00007fff 0x3a618060 0x00007f9a 	..:.....`.a:....
	0x7f3af440 0x00007fff 0x00005813 0x00000001 	@.:......X......
	...
	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoTimer | __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__ | _ZN7WebCoreL10timerFiredEP16__CFRunLoopTimerPv SharedTimerMac.mm:167 | WebCore::ThreadTimers::sharedTimerFired() ThreadTimers.cpp:94 | WebCore::ThreadTimers::sharedTimerFiredInternal() ThreadTimers.cpp:118 | WebCore::Timer<WebCore::DocumentLoader>::fired() Timer.h:100 | WebCore::DocumentLoader::substituteResourceDeliveryTimerFired(WebCore::Timer<WebCore::DocumentLoader>*) DocumentLoader.cpp:600 | WebCore::SubresourceLoader::didFinishLoading(double) SubresourceLoader.cpp:191 | WebCore::CachedResourceRequest::didFinishLoading(WebCore::SubresourceLoader*, double) CachedResourceRequest.cpp:196 | WebCore::CachedRawResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedRawResource.cpp:67 | WebCore::CachedResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedResource.cpp:166 | WebCore::CachedResource::checkNotify() CachedResource.cpp:156 | non-virtual thunk to WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) | WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) DocumentThreadableLoader.cpp:262 | WebCore::DocumentThreadableLoader::didFinishLoading(unsigned long, double) DocumentThreadableLoader.cpp:277 | non-virtual thunk to WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) | WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) XMLHttpRequest.cpp:1008 | WebCore::XMLHttpRequest::changeState(WebCore::XMLHttpRequest::State) XMLHttpRequest.cpp:329 | WebCore::XMLHttpRequest::callReadyStateChangeListener() XMLHttpRequest.cpp:345 | WebCore::XMLHttpRequestProgressEventThrottle::dispatchEvent(WTF::PassRefPtr<WebCore::Event>, WebCore::ProgressEventAction) XMLHttpRequestProgressEventThrottle.cpp:81 | WebCore::EventTarget::dispatchEvent(WTF::PassRefPtr<WebCore::Event>) EventTarget.cpp:176 | WebCore::EventTarget::fireEventListeners(WebCore::Event*) EventTarget.cpp:199 | WebCore::EventTarget::fireEventListeners(WebCore::Event*, WebCore::EventTargetData*, WTF::Vector<WebCore::RegisteredEventListener, 1ul>&) EventTarget.cpp:214 | WebCore::JSEventListener::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) JSEventListener.cpp:128 | WebCore::JSMainThreadExecState::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) JSMainThreadExecState.h:52 | JSC::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) CallData.cpp:39 | JSC::Interpreter::executeCall(JSC::ExecState*, JSC::JSObject*, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) Interpreter.cpp:986 | JSC::JITCode::execute(JSC::RegisterFile*, JSC::ExecState*, JSC::JSGlobalData*) JITCode.h:115 | 0x2298f4c011f8 | WebCore::jsXMLHttpRequestPrototypeFunctionSend(JSC::ExecState*) JSXMLHttpRequest.cpp:604 | WebCore::JSXMLHttpRequest::send(JSC::ExecState*) JSXMLHttpRequestCustom.cpp:132 | WebCore::XMLHttpRequest::send(WTF::String const&, int&) XMLHttpRequest.cpp:544 | WebCore::XMLHttpRequest::createRequest(int&) XMLHttpRequest.cpp:665 | WebCore::ThreadableLoader::create(WebCore::ScriptExecutionContext*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) ThreadableLoader.cpp:54 | WebCore::DocumentThreadableLoader::create(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:65 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:111 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:88 | WebCore::DocumentThreadableLoader::loadRequest(WebCore::ResourceRequest const&, WebCore::SecurityCheckPolicy) DocumentThreadableLoader.cpp:337 | WebCore::CachedResourceLoader::requestRawResource(WebCore::ResourceRequest&, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:225 | WebCore::CachedResourceLoader::requestResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoaderOptions const&, WebCore::ResourceLoadPriority, bool) CachedResourceLoader.cpp:400 | WebCore::CachedResourceLoader::loadResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:469 | WebCore::CachedResource::load(WebCore::CachedResourceLoader*, WebCore::ResourceLoaderOptions const&) CachedResource.cpp:142 | WebCore::CachedResourceRequest::load(WebCore::CachedResourceLoader*, WebCore::CachedResource*, WebCore::ResourceLoaderOptions const&) CachedResourceRequest.cpp:135 | WebCore::ResourceLoadScheduler::scheduleSubresourceLoad(WebCore::Frame*, WebCore::SubresourceLoaderClient*, WebCore::ResourceRequest const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) ResourceLoadScheduler.cpp:92 | WebCore::ResourceLoadScheduler::scheduleLoad(WebCore::ResourceLoader*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:132 | WebCore::ResourceLoadScheduler::servePendingRequests(WebCore::ResourceLoadScheduler::HostInformation*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:210 | WebCore::ResourceLoader::start() ResourceLoader.cpp:162 | WebCore::ResourceHandle::create(WebCore::NetworkingContext*, WebCore::ResourceRequest const&, WebCore::ResourceHandleClient*, bool, bool) ResourceHandle.cpp:71 | WebCore::ResourceHandle::start(WebCore::NetworkingContext*) ResourceHandleMac.mm:278 | WebCore::ResourceHandle::createNSURLConnection(objc_object*, bool, bool) ResourceHandleMac.mm:238 | -[NSURLConnection(NSURLConnectionPrivate) _initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:] | CFURLConnectionCreateWithProperties | URLConnection::initialize(_CFURLRequest const*, CFURLConnectionClient_V1*, __CFDictionary const*) | CFObject::Allocate(unsigned long, CFClass const&, __CFAllocator const*) | _CFRuntimeCreateInstance | malloc_zone_malloc 
EOF

my $expectedOutput =
[
  {
    'leaksOutput' => 'Process:         DumpRenderTree [29903]'
  },
  {
    'leaksOutput' => 'Path:            /Volumes/Data/Build/Debug/DumpRenderTree'
  },
  {
    'leaksOutput' => 'Load Address:    0x102116000'
  },
  {
    'leaksOutput' => 'Identifier:      DumpRenderTree'
  },
  {
    'leaksOutput' => 'Version:         ??? (???)'
  },
  {
    'leaksOutput' => 'Code Type:       X86-64 (Native)'
  },
  {
    'leaksOutput' => 'Parent Process:  Python [29892]'
  },
  {
    'leaksOutput' => ''
  },
  {
    'leaksOutput' => 'Date/Time:       2011-11-14 11:12:45.706 -0800'
  },
  {
    'leaksOutput' => 'OS Version:      Mac OS X 10.7.2 (11C74)'
  },
  {
    'leaksOutput' => 'Report Version:  7'
  },
  {
    'leaksOutput' => ''
  },
  {
    'leaksOutput' => 'leaks Report Version:  2.0'
  },
  {
    'leaksOutput' => 'leaks(12871,0xacdfa2c0) malloc: process 89617 no longer exists, stack logs deleted from /tmp/stack-logs.89617.DumpRenderTree.A2giy6.index'
  },
  {
    'leaksOutput' => 'Process 29903: 60015 nodes malloced for 7290 KB'
  },
  {
    'leaksOutput' => 'Process 29903: 2 leaks for 1008 total leaked bytes.'
  },
  {
    'leaksOutput' => join('', split(/\n/, <<EOF)),
Leak: 0x7f9a3a612810  size=576  zone: DefaultMallocZone_0x10227b000   URLConnectionLoader::LoaderConnectionEventQueue  C++  CFNetwork
	0x7f3af460 0x00007fff 0x7edf2f40 0x00007fff 	`.:.....@/.~....
	0x7f3af488 0x00007fff 0xdab071b1 0x0000f068 	..:......q..h...
	0x0100000a 0x00000000 0x7edf3f50 0x00007fff 	........P?.~....
	0x00000000 0x00000000 0xdab071cc 0x0000f068 	.........q..h...
	0x01000010 0x00000000 0x3a616210 0x00007f9a 	.........ba:....
	0x00000000 0x00000000 0xdab071e5 0x0000f068 	.........q..h...
	0x00000000 0x00000000 0x00000000 0x00000000 	................
	0x00000000 0x00000000 0xdab07245 0x0000f068 	........Er..h...
	...
	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoSources0 | __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__ | MultiplexerSource::perform() | URLConnectionClient::processEvents() | URLConnectionClient::ClientConnectionEventQueue::processAllEventsAndConsumePayload(XConnectionEventInfo<XClientEvent, XClientEventParams>*, long) | URLConnectionClient::_clientWillSendRequest(_CFURLRequest const*, _CFURLResponse*, URLConnectionClient::ClientConnectionEventQueue*) | URLConnectionClient::getRequestForTransmission(unsigned char, _CFURLResponse*, _CFURLRequest const*, __CFError**) | URLConnectionLoader::pushLoaderEvent(XConnectionEventInfo<XLoaderEvent, XLoaderEventParams>*) | CFAllocatedObject::operator new(unsigned long, __CFAllocator const*) | malloc_zone_malloc 
EOF
    'callStack' => 
'	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoSources0 | __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__ | MultiplexerSource::perform() | URLConnectionClient::processEvents() | URLConnectionClient::ClientConnectionEventQueue::processAllEventsAndConsumePayload(XConnectionEventInfo<XClientEvent, XClientEventParams>*, long) | URLConnectionClient::_clientWillSendRequest(_CFURLRequest const*, _CFURLResponse*, URLConnectionClient::ClientConnectionEventQueue*) | URLConnectionClient::getRequestForTransmission(unsigned char, _CFURLResponse*, _CFURLRequest const*, __CFError**) | URLConnectionLoader::pushLoaderEvent(XConnectionEventInfo<XLoaderEvent, XLoaderEventParams>*) | CFAllocatedObject::operator new(unsigned long, __CFAllocator const*) | malloc_zone_malloc ',
    'address' => '0x7f9a3a612810',
    'size' => '576',
    'type' => '',
  },

  {
    'leaksOutput' => join('', split(/\n/, <<EOF)),
Leak: 0x7f9a3a618090  size=432  zone: DefaultMallocZone_0x10227b000   URLConnectionInstanceData  CFType  CFNetwork
	0x7edcab28 0x00007fff 0x00012b80 0x00000001 	(..~.....+......
	0x7f3af310 0x00007fff 0x7f3af3f8 0x00007fff 	..:.......:.....
	0x4d555458 0x00000000 0x00000000 0x00002068 	XTUM........h ..
	0x00000000 0x00000000 0x00000c00 0x00000c00 	................
	0x00000000 0x00000000 0x3a6180c8 0x00007f9a 	..........a:....
	0x3a6180cc 0x00007f9a 0x00000000 0x00000000 	..a:............
	0x7f3af418 0x00007fff 0x3a618060 0x00007f9a 	..:.....`.a:....
	0x7f3af440 0x00007fff 0x00005813 0x00000001 	@.:......X......
	...
	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoTimer | __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__ | _ZN7WebCoreL10timerFiredEP16__CFRunLoopTimerPv SharedTimerMac.mm:167 | WebCore::ThreadTimers::sharedTimerFired() ThreadTimers.cpp:94 | WebCore::ThreadTimers::sharedTimerFiredInternal() ThreadTimers.cpp:118 | WebCore::Timer<WebCore::DocumentLoader>::fired() Timer.h:100 | WebCore::DocumentLoader::substituteResourceDeliveryTimerFired(WebCore::Timer<WebCore::DocumentLoader>*) DocumentLoader.cpp:600 | WebCore::SubresourceLoader::didFinishLoading(double) SubresourceLoader.cpp:191 | WebCore::CachedResourceRequest::didFinishLoading(WebCore::SubresourceLoader*, double) CachedResourceRequest.cpp:196 | WebCore::CachedRawResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedRawResource.cpp:67 | WebCore::CachedResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedResource.cpp:166 | WebCore::CachedResource::checkNotify() CachedResource.cpp:156 | non-virtual thunk to WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) | WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) DocumentThreadableLoader.cpp:262 | WebCore::DocumentThreadableLoader::didFinishLoading(unsigned long, double) DocumentThreadableLoader.cpp:277 | non-virtual thunk to WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) | WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) XMLHttpRequest.cpp:1008 | WebCore::XMLHttpRequest::changeState(WebCore::XMLHttpRequest::State) XMLHttpRequest.cpp:329 | WebCore::XMLHttpRequest::callReadyStateChangeListener() XMLHttpRequest.cpp:345 | WebCore::XMLHttpRequestProgressEventThrottle::dispatchEvent(WTF::PassRefPtr<WebCore::Event>, WebCore::ProgressEventAction) XMLHttpRequestProgressEventThrottle.cpp:81 | WebCore::EventTarget::dispatchEvent(WTF::PassRefPtr<WebCore::Event>) EventTarget.cpp:176 | WebCore::EventTarget::fireEventListeners(WebCore::Event*) EventTarget.cpp:199 | WebCore::EventTarget::fireEventListeners(WebCore::Event*, WebCore::EventTargetData*, WTF::Vector<WebCore::RegisteredEventListener, 1ul>&) EventTarget.cpp:214 | WebCore::JSEventListener::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) JSEventListener.cpp:128 | WebCore::JSMainThreadExecState::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) JSMainThreadExecState.h:52 | JSC::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) CallData.cpp:39 | JSC::Interpreter::executeCall(JSC::ExecState*, JSC::JSObject*, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) Interpreter.cpp:986 | JSC::JITCode::execute(JSC::RegisterFile*, JSC::ExecState*, JSC::JSGlobalData*) JITCode.h:115 | 0x2298f4c011f8 | WebCore::jsXMLHttpRequestPrototypeFunctionSend(JSC::ExecState*) JSXMLHttpRequest.cpp:604 | WebCore::JSXMLHttpRequest::send(JSC::ExecState*) JSXMLHttpRequestCustom.cpp:132 | WebCore::XMLHttpRequest::send(WTF::String const&, int&) XMLHttpRequest.cpp:544 | WebCore::XMLHttpRequest::createRequest(int&) XMLHttpRequest.cpp:665 | WebCore::ThreadableLoader::create(WebCore::ScriptExecutionContext*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) ThreadableLoader.cpp:54 | WebCore::DocumentThreadableLoader::create(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:65 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:111 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:88 | WebCore::DocumentThreadableLoader::loadRequest(WebCore::ResourceRequest const&, WebCore::SecurityCheckPolicy) DocumentThreadableLoader.cpp:337 | WebCore::CachedResourceLoader::requestRawResource(WebCore::ResourceRequest&, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:225 | WebCore::CachedResourceLoader::requestResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoaderOptions const&, WebCore::ResourceLoadPriority, bool) CachedResourceLoader.cpp:400 | WebCore::CachedResourceLoader::loadResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:469 | WebCore::CachedResource::load(WebCore::CachedResourceLoader*, WebCore::ResourceLoaderOptions const&) CachedResource.cpp:142 | WebCore::CachedResourceRequest::load(WebCore::CachedResourceLoader*, WebCore::CachedResource*, WebCore::ResourceLoaderOptions const&) CachedResourceRequest.cpp:135 | WebCore::ResourceLoadScheduler::scheduleSubresourceLoad(WebCore::Frame*, WebCore::SubresourceLoaderClient*, WebCore::ResourceRequest const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) ResourceLoadScheduler.cpp:92 | WebCore::ResourceLoadScheduler::scheduleLoad(WebCore::ResourceLoader*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:132 | WebCore::ResourceLoadScheduler::servePendingRequests(WebCore::ResourceLoadScheduler::HostInformation*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:210 | WebCore::ResourceLoader::start() ResourceLoader.cpp:162 | WebCore::ResourceHandle::create(WebCore::NetworkingContext*, WebCore::ResourceRequest const&, WebCore::ResourceHandleClient*, bool, bool) ResourceHandle.cpp:71 | WebCore::ResourceHandle::start(WebCore::NetworkingContext*) ResourceHandleMac.mm:278 | WebCore::ResourceHandle::createNSURLConnection(objc_object*, bool, bool) ResourceHandleMac.mm:238 | -[NSURLConnection(NSURLConnectionPrivate) _initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:] | CFURLConnectionCreateWithProperties | URLConnection::initialize(_CFURLRequest const*, CFURLConnectionClient_V1*, __CFDictionary const*) | CFObject::Allocate(unsigned long, CFClass const&, __CFAllocator const*) | _CFRuntimeCreateInstance | malloc_zone_malloc 
EOF
    'callStack' => 
'	Call stack: [thread 0x7fff7e3b4960]: | start | main DumpRenderTree.mm:835 | dumpRenderTree(int, char const**) DumpRenderTree.mm:794 | _ZL20runTestingServerLoopv DumpRenderTree.mm:744 | _ZL7runTestRKSs DumpRenderTree.mm:1273 | -[NSRunLoop(NSRunLoop) runMode:beforeDate:] | CFRunLoopRunSpecific | __CFRunLoopRun | __CFRunLoopDoTimer | __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__ | _ZN7WebCoreL10timerFiredEP16__CFRunLoopTimerPv SharedTimerMac.mm:167 | WebCore::ThreadTimers::sharedTimerFired() ThreadTimers.cpp:94 | WebCore::ThreadTimers::sharedTimerFiredInternal() ThreadTimers.cpp:118 | WebCore::Timer<WebCore::DocumentLoader>::fired() Timer.h:100 | WebCore::DocumentLoader::substituteResourceDeliveryTimerFired(WebCore::Timer<WebCore::DocumentLoader>*) DocumentLoader.cpp:600 | WebCore::SubresourceLoader::didFinishLoading(double) SubresourceLoader.cpp:191 | WebCore::CachedResourceRequest::didFinishLoading(WebCore::SubresourceLoader*, double) CachedResourceRequest.cpp:196 | WebCore::CachedRawResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedRawResource.cpp:67 | WebCore::CachedResource::data(WTF::PassRefPtr<WebCore::SharedBuffer>, bool) CachedResource.cpp:166 | WebCore::CachedResource::checkNotify() CachedResource.cpp:156 | non-virtual thunk to WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) | WebCore::DocumentThreadableLoader::notifyFinished(WebCore::CachedResource*) DocumentThreadableLoader.cpp:262 | WebCore::DocumentThreadableLoader::didFinishLoading(unsigned long, double) DocumentThreadableLoader.cpp:277 | non-virtual thunk to WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) | WebCore::XMLHttpRequest::didFinishLoading(unsigned long, double) XMLHttpRequest.cpp:1008 | WebCore::XMLHttpRequest::changeState(WebCore::XMLHttpRequest::State) XMLHttpRequest.cpp:329 | WebCore::XMLHttpRequest::callReadyStateChangeListener() XMLHttpRequest.cpp:345 | WebCore::XMLHttpRequestProgressEventThrottle::dispatchEvent(WTF::PassRefPtr<WebCore::Event>, WebCore::ProgressEventAction) XMLHttpRequestProgressEventThrottle.cpp:81 | WebCore::EventTarget::dispatchEvent(WTF::PassRefPtr<WebCore::Event>) EventTarget.cpp:176 | WebCore::EventTarget::fireEventListeners(WebCore::Event*) EventTarget.cpp:199 | WebCore::EventTarget::fireEventListeners(WebCore::Event*, WebCore::EventTargetData*, WTF::Vector<WebCore::RegisteredEventListener, 1ul>&) EventTarget.cpp:214 | WebCore::JSEventListener::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) JSEventListener.cpp:128 | WebCore::JSMainThreadExecState::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) JSMainThreadExecState.h:52 | JSC::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) CallData.cpp:39 | JSC::Interpreter::executeCall(JSC::ExecState*, JSC::JSObject*, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&) Interpreter.cpp:986 | JSC::JITCode::execute(JSC::RegisterFile*, JSC::ExecState*, JSC::JSGlobalData*) JITCode.h:115 | 0x2298f4c011f8 | WebCore::jsXMLHttpRequestPrototypeFunctionSend(JSC::ExecState*) JSXMLHttpRequest.cpp:604 | WebCore::JSXMLHttpRequest::send(JSC::ExecState*) JSXMLHttpRequestCustom.cpp:132 | WebCore::XMLHttpRequest::send(WTF::String const&, int&) XMLHttpRequest.cpp:544 | WebCore::XMLHttpRequest::createRequest(int&) XMLHttpRequest.cpp:665 | WebCore::ThreadableLoader::create(WebCore::ScriptExecutionContext*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) ThreadableLoader.cpp:54 | WebCore::DocumentThreadableLoader::create(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:65 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:111 | WebCore::DocumentThreadableLoader::DocumentThreadableLoader(WebCore::Document*, WebCore::ThreadableLoaderClient*, WebCore::DocumentThreadableLoader::BlockingBehavior, WebCore::ResourceRequest const&, WebCore::ThreadableLoaderOptions const&) DocumentThreadableLoader.cpp:88 | WebCore::DocumentThreadableLoader::loadRequest(WebCore::ResourceRequest const&, WebCore::SecurityCheckPolicy) DocumentThreadableLoader.cpp:337 | WebCore::CachedResourceLoader::requestRawResource(WebCore::ResourceRequest&, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:225 | WebCore::CachedResourceLoader::requestResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoaderOptions const&, WebCore::ResourceLoadPriority, bool) CachedResourceLoader.cpp:400 | WebCore::CachedResourceLoader::loadResource(WebCore::CachedResource::Type, WebCore::ResourceRequest&, WTF::String const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) CachedResourceLoader.cpp:469 | WebCore::CachedResource::load(WebCore::CachedResourceLoader*, WebCore::ResourceLoaderOptions const&) CachedResource.cpp:142 | WebCore::CachedResourceRequest::load(WebCore::CachedResourceLoader*, WebCore::CachedResource*, WebCore::ResourceLoaderOptions const&) CachedResourceRequest.cpp:135 | WebCore::ResourceLoadScheduler::scheduleSubresourceLoad(WebCore::Frame*, WebCore::SubresourceLoaderClient*, WebCore::ResourceRequest const&, WebCore::ResourceLoadPriority, WebCore::ResourceLoaderOptions const&) ResourceLoadScheduler.cpp:92 | WebCore::ResourceLoadScheduler::scheduleLoad(WebCore::ResourceLoader*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:132 | WebCore::ResourceLoadScheduler::servePendingRequests(WebCore::ResourceLoadScheduler::HostInformation*, WebCore::ResourceLoadPriority) ResourceLoadScheduler.cpp:210 | WebCore::ResourceLoader::start() ResourceLoader.cpp:162 | WebCore::ResourceHandle::create(WebCore::NetworkingContext*, WebCore::ResourceRequest const&, WebCore::ResourceHandleClient*, bool, bool) ResourceHandle.cpp:71 | WebCore::ResourceHandle::start(WebCore::NetworkingContext*) ResourceHandleMac.mm:278 | WebCore::ResourceHandle::createNSURLConnection(objc_object*, bool, bool) ResourceHandleMac.mm:238 | -[NSURLConnection(NSURLConnectionPrivate) _initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:] | CFURLConnectionCreateWithProperties | URLConnection::initialize(_CFURLRequest const*, CFURLConnectionClient_V1*, __CFDictionary const*) | CFObject::Allocate(unsigned long, CFClass const&, __CFAllocator const*) | _CFRuntimeCreateInstance | malloc_zone_malloc ',
    'address' => '0x7f9a3a618090',
    'size' => '432',
    'type' => '',
  },
];

my $actualOutput = RunLeaks::parseLeaksOutput(@input);

plan(tests => 1);
is_deeply($actualOutput, $expectedOutput, "leaks Report Version 2.0 (new)");

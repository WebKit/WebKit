#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'includes': [
        '../../WebCore/WebCore.gypi',
        'WebKit.gypi',
        'features.gypi',
    ],
    'variables': {
        'conditions': [
            # Location of the chromium src directory and target type is different
            # if webkit is built inside chromium or as standalone project.
            ['inside_chromium_build==0', {
                # Webkit is being built outside of the full chromium project.
                # e.g. via build-webkit --chromium
                'chromium_src_dir': '../../WebKit/chromium',
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '../../../../..',
            }],
        ],
        'ahem_path': '../../../Tools/DumpRenderTree/qt/fonts/AHEM____.TTF',

        # If debug_devtools is set to 1, JavaScript files for DevTools are
        # stored as is. Otherwise, a concatenated file is stored.
        'debug_devtools%': 0,

        # List of DevTools source files, ordered by dependencies. It is used both
        # for copying them to resource dir, and for generating 'devtools.html' file.
        'devtools_files': [
            '<@(devtools_css_files)',
            '<@(devtools_js_files)',
        ],
    },
    'targets': [
        {
            'target_name': 'webkit',
            'type': 'static_library',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'dependencies': [
                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                '../../Platform/Platform.gyp/Platform.gyp:webkit_platform', # actually WebCore should depend on this
                '<(chromium_src_dir)/skia/skia.gyp:skia',
                '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
                '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:translator_glsl',
                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
            ],
            'export_dependent_settings': [
                '<(chromium_src_dir)/skia/skia.gyp:skia',
                '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
            ],
            'include_dirs': [
                'public',
                'src',
                '<(chromium_src_dir)/third_party/angle/include',
                '<(chromium_src_dir)/third_party/skia/include/utils',
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION=1',
            ],
            'sources': [
                'bridge/PeerConnectionHandler.cpp',
                'bridge/PeerConnectionHandlerInternal.cpp',
                'bridge/PeerConnectionHandlerInternal.h',
                'public/WebAccessibilityNotification.h',
                'public/WebAccessibilityObject.h',
                'public/WebAccessibilityRole.h',
                'public/WebAnimationController.h',
                'public/WebApplicationCacheHostClient.h',
                'public/WebArrayBuffer.h',
                'public/WebAttribute.h',
                'public/WebAudioSourceProvider.h',
                'public/WebAudioSourceProviderClient.h',
                'public/WebAutofillClient.h',
                'public/WebBindings.h',
                'public/WebBlob.h',
                'public/WebCache.h',
                'public/WebColorChooser.h',
                'public/WebColorChooserClient.h',
                'public/WebCommonWorkerClient.h',
                'public/WebCompositionUnderline.h',
                'public/WebCompositor.h',
                'public/WebCompositorClient.h',
                'public/WebCompositorInputHandler.h',
                'public/WebCompositorInputHandlerClient.h',
                'public/WebConsoleMessage.h',
                'public/WebContextMenuData.h',
                'public/WebCrossOriginPreflightResultCache.h',
                'public/WebCursorInfo.h',
                'public/WebDOMEvent.h',
                'public/WebDOMEventListener.h',
                'public/WebDOMMessageEvent.h',
                'public/WebDOMMouseEvent.h',
                'public/WebDOMMutationEvent.h',
                'public/WebDOMStringList.h',
                'public/WebDataSource.h',
                'public/WebDatabase.h',
                'public/WebDatabaseObserver.h',
                'public/WebDevToolsAgent.h',
                'public/WebDevToolsAgentClient.h',
                'public/WebDevToolsFrontend.h',
                'public/WebDevToolsFrontendClient.h',
                'public/WebDeviceOrientation.h',
                'public/WebDeviceOrientationClient.h',
                'public/WebDeviceOrientationClientMock.h',
                'public/WebDeviceOrientationController.h',
                'public/WebDocument.h',
                'public/WebDocumentType.h',
                'public/WebEditingAction.h',
                'public/WebElement.h',
                'public/WebExceptionCode.h',
                'public/WebExternalPopupMenu.h',
                'public/WebExternalPopupMenuClient.h',
                'public/WebFileChooserCompletion.h',
                'public/WebFileChooserParams.h',
                'public/WebFileError.h',
                'public/WebFileInfo.h',
                'public/WebFileSystemCallbacks.h',
                'public/WebFileSystemEntry.h',
                'public/WebFileUtilities.h',
                'public/WebFileWriter.h',
                'public/WebFileWriterClient.h',
                'public/WebFindOptions.h',
                'public/WebFont.h',
                'public/WebFontCache.h',
                'public/WebFontDescription.h',
                'public/WebFormControlElement.h',
                'public/WebFormElement.h',
                'public/WebFrame.h',
                'public/WebFrameClient.h',
                'public/WebGeolocationClient.h',
                'public/WebGeolocationClientMock.h',
                'public/WebGeolocationController.h',
                'public/WebGeolocationError.h',
                'public/WebGeolocationPermissionRequest.h',
                'public/WebGeolocationPermissionRequestManager.h',
                'public/WebGeolocationPosition.h',
                'public/WebGlyphCache.h',
                'public/WebHistoryItem.h',
                'public/WebIDBCallbacks.h',
                'public/WebIDBCursor.h',
                'public/WebIDBDatabase.h',
                'public/WebIDBDatabaseCallbacks.h',
                'public/WebIDBDatabaseError.h',
                'public/WebIDBFactory.h',
                'public/WebIDBIndex.h',
                'public/WebIDBKey.h',
                'public/WebIDBKeyPath.h',
                'public/WebIDBKeyRange.h',
                'public/WebIDBObjectStore.h',
                'public/WebIDBTransaction.h',
                'public/WebIDBTransactionCallbacks.h',
                'public/WebIconLoadingCompletion.h',
                'public/WebIconURL.h',
                'public/WebImageDecoder.h',
                'public/WebInputElement.h',
                'public/WebInputEvent.h',
                'public/WebIntent.h',
                'public/WebIntentServiceInfo.h',
                'public/WebKit.h',
                'public/WebLabelElement.h',
                'public/WebMediaElement.h',
                'public/WebMediaPlayer.h',
                'public/WebMediaPlayerAction.h',
                'public/WebMediaPlayerClient.h',
                'public/WebMediaStreamRegistry.h',
                'public/WebMenuItemInfo.h',
                'public/WebMessagePortChannel.h',
                'public/WebMessagePortChannelClient.h',
                'public/WebNamedNodeMap.h',
                'public/WebNavigationType.h',
                'public/WebNetworkStateNotifier.h',
                'public/WebNode.h',
                'public/WebNodeCollection.h',
                'public/WebNodeList.h',
                'public/WebNotification.h',
                'public/WebNotificationPermissionCallback.h',
                'public/WebNotificationPresenter.h',
                'public/WebOptionElement.h',
                'public/WebPageOverlay.h',
                'public/WebPageSerializer.h',
                'public/WebPageSerializerClient.h',
                'public/WebPageVisibilityState.h',
                'public/WebPasswordFormData.h',
                'public/WebPerformance.h',
                'public/WebPermissionClient.h',
                'public/WebPlugin.h',
                'public/WebPluginContainer.h',
                'public/WebPluginDocument.h',
                'public/WebPluginListBuilder.h',
                'public/WebPopupMenu.h',
                'public/WebPopupMenuInfo.h',
                'public/WebPopupType.h',
                'public/WebRange.h',
                'public/WebReferrerPolicy.h',
                'public/WebRegularExpression.h',
                'public/WebRuntimeFeatures.h',
                'public/WebScreenInfo.h',
                'public/WebScriptController.h',
                'public/WebScriptSource.h',
                'public/WebScrollbar.h',
                'public/WebScrollbarClient.h',
                'public/WebSearchableFormData.h',
                'public/WebSecurityOrigin.h',
                'public/WebSecurityPolicy.h',
                'public/WebSelectElement.h',
                'public/WebSettings.h',
                'public/WebSharedWorkerClient.h',
                'public/WebSharedWorker.h',
                'public/WebSharedWorkerRepository.h',
                'public/WebSocket.h',
                'public/WebSocketClient.h',
                'public/WebSpeechInputController.h',
                'public/WebSpeechInputControllerMock.h',
                'public/WebSpeechInputListener.h',
                'public/WebSpeechInputResult.h',
                'public/WebSpellCheckClient.h',
                'public/WebStorageArea.h',
                'public/WebStorageEventDispatcher.h',
                'public/WebStorageNamespace.h',
                'public/WebStorageQuotaCallbacks.h',
                'public/WebStorageQuotaType.h',
                'public/WebTextAffinity.h',
                'public/WebTextCaseSensitivity.h',
                'public/WebTextCheckingCompletion.h',
                'public/WebTextCheckingResult.h',
                'public/WebTextDirection.h',
                'public/WebTextInputType.h',
                'public/WebTextRun.h',
                'public/WebURLLoaderOptions.h',
                'public/WebUserMediaClient.h',
                'public/WebUserMediaRequest.h',
                'public/WebView.h',
                'public/WebViewClient.h',
                'public/WebWidget.h',
                'public/WebWidgetClient.h',
                'public/WebWorker.h',
                'public/WebWorkerRunLoop.h',
                'public/android/WebInputEventFactory.h',
                'public/android/WebSandboxSupport.h',
                'public/gtk/WebInputEventFactory.h',
                'public/linux/WebFontRenderStyle.h',
                'public/linux/WebFontRendering.h',
                'public/linux/WebRenderTheme.h',
                'public/mac/WebInputEventFactory.h',
                'public/mac/WebSandboxSupport.h',
                'public/mac/WebScreenInfoFactory.h',
                'public/mac/WebSubstringUtil.h',
                'public/platform/WebArrayBufferView.h',
                'public/platform/WebAudioBus.h',
                'public/platform/WebAudioDevice.h',
                'public/platform/WebBlobData.h',
                'public/platform/WebBlobRegistry.h',
                'public/platform/WebCanvas.h',
                'public/platform/WebClipboard.h',
                'public/platform/WebColor.h',
                'public/platform/WebColorName.h',
                'public/platform/WebCommon.h',
                'public/platform/WebContentLayer.h',
                'public/platform/WebContentLayerClient.h',
                'public/platform/WebCookie.h',
                'public/platform/WebCookieJar.h',
                'public/platform/WebData.h',
                'public/platform/WebDragData.h',
                'public/platform/WebExternalTextureLayer.h',
                'public/platform/WebFileSystem.h',
                'public/platform/WebFloatPoint.h',
                'public/platform/WebFloatQuad.h',
                'public/platform/WebFloatRect.h',
                'public/platform/WebGamepad.h',
                'public/platform/WebGamepads.h',
                'public/platform/WebGraphicsContext3D.h',
                'public/platform/WebHTTPBody.h',
                'public/platform/WebHTTPHeaderVisitor.h',
                'public/platform/WebHTTPLoadInfo.h',
                'public/platform/WebImage.h',
                'public/platform/WebKitPlatformSupport.h',
                'public/platform/WebLayer.h',
                'public/platform/WebLayerTreeView.h',
                'public/platform/WebLayerTreeViewClient.h',
                'public/platform/WebLocalizedString.h',
                'public/platform/WebMediaStreamDescriptor.h',
                'public/platform/WebMediaStreamSource.h',
                'public/platform/WebNonCopyable.h',
                'public/platform/WebPeerConnectionHandler.h',
                'public/platform/WebPeerConnectionHandlerClient.h',
                'public/platform/WebPoint.h',
                'public/platform/WebPrivateOwnPtr.h',
                'public/platform/WebPrivatePtr.h',
                'public/platform/WebRect.h',
                'public/platform/WebSerializedScriptValue.h',
                'public/platform/WebSize.h',
                'public/platform/WebSocketStreamError.h',
                'public/platform/WebSocketStreamHandle.h',
                'public/platform/WebSocketStreamHandleClient.h',
                'public/platform/WebString.h',
                'public/platform/WebThread.h',
                'public/platform/WebThreadSafeData.h',
                'public/platform/WebURL.h',
                'public/platform/WebURLError.h',
                'public/platform/WebURLLoadTiming.h',
                'public/platform/WebURLLoader.h',
                'public/platform/WebURLLoaderClient.h',
                'public/platform/WebURLRequest.h',
                'public/platform/WebURLResponse.h',
                'public/platform/WebVector.h',
                'public/platform/android/WebSandboxSupport.h',
                'public/platform/android/WebThemeEngine.h',
                'public/platform/linux/WebThemeEngine.h',
                'public/platform/mac/WebSandboxSupport.h',
                'public/platform/mac/WebThemeEngine.h',
                'public/platform/win/WebSandboxSupport.h',
                'public/platform/win/WebThemeEngine.h',
                'public/win/WebInputEventFactory.h',
                'public/win/WebSandboxSupport.h',
                'public/win/WebScreenInfoFactory.h',
                'public/x11/WebScreenInfoFactory.h',
                'src/ApplicationCacheHost.cpp',
                'src/ApplicationCacheHostInternal.h',
                'src/AssertMatchingEnums.cpp',
                'src/AssociatedURLLoader.cpp',
                'src/AssociatedURLLoader.h',
                'src/AsyncFileSystemChromium.cpp',
                'src/AsyncFileSystemChromium.h',
                'src/AsyncFileWriterChromium.cpp',
                'src/AsyncFileWriterChromium.h',
                'src/AudioDestinationChromium.cpp',
                'src/AudioDestinationChromium.h',
                'src/AutofillPopupMenuClient.cpp',
                'src/AutofillPopupMenuClient.h',
                'src/BackForwardListChromium.cpp',
                'src/BackForwardListChromium.h',
                'src/BlobRegistryProxy.cpp',
                'src/BlobRegistryProxy.h',
                'src/BoundObject.cpp',
                'src/BoundObject.h',
                'src/CCThreadImpl.cpp',
                'src/CCThreadImpl.h',
                'src/ChromeClientImpl.cpp',
                'src/ChromeClientImpl.h',
                'src/ColorChooserProxy.cpp',
                'src/ColorChooserProxy.h',
                'src/ChromiumCurrentTime.cpp',
                'src/ChromiumOSRandomSource.cpp',
                'src/ChromiumThreading.cpp',
                'src/CompositionUnderlineBuilder.h',
                'src/CompositionUnderlineVectorBuilder.cpp',
                'src/CompositionUnderlineVectorBuilder.h',
                'src/ContextMenuClientImpl.cpp',
                'src/ContextMenuClientImpl.h',
                'src/DatabaseObserver.cpp',
                'src/DeviceOrientationClientProxy.cpp',
                'src/DeviceOrientationClientProxy.h',
                'src/DOMUtilitiesPrivate.cpp',
                'src/DOMUtilitiesPrivate.h',
                'src/DragClientImpl.cpp',
                'src/DragClientImpl.h',
                'src/DragScrollTimer.cpp',
                'src/DragScrollTimer.h',
                'src/EditorClientImpl.cpp',
                'src/EditorClientImpl.h',
                'src/EventListenerWrapper.cpp',
                'src/EventListenerWrapper.h',
                'src/Extensions3DChromium.cpp',
                'src/ExternalPopupMenu.cpp',
                'src/ExternalPopupMenu.h',
                'src/FrameLoaderClientImpl.cpp',
                'src/FrameLoaderClientImpl.h',
                'src/FrameNetworkingContextImpl.h',
                'src/GeolocationClientProxy.cpp',
                'src/GeolocationClientProxy.h',
                'src/GraphicsContext3DChromium.cpp',
                'src/GraphicsContext3DPrivate.h',
                'src/gtk/WebInputEventFactory.cpp',
                'src/IDBCallbacksProxy.cpp',
                'src/IDBCallbacksProxy.h',
                'src/IDBCursorBackendProxy.cpp',
                'src/IDBCursorBackendProxy.h',
                'src/IDBDatabaseCallbacksProxy.cpp',
                'src/IDBDatabaseCallbacksProxy.h',
                'src/IDBDatabaseBackendProxy.cpp',
                'src/IDBDatabaseBackendProxy.h',
                'src/IDBFactoryBackendProxy.cpp',
                'src/IDBFactoryBackendProxy.h',
                'src/IDBIndexBackendProxy.cpp',
                'src/IDBIndexBackendProxy.h',
                'src/IDBObjectStoreBackendProxy.cpp',
                'src/IDBObjectStoreBackendProxy.h',
                'src/IDBTransactionBackendProxy.cpp',
                'src/IDBTransactionBackendProxy.h',
                'src/IDBTransactionCallbacksProxy.cpp',
                'src/IDBTransactionCallbacksProxy.h',
                'src/InspectorClientImpl.cpp',
                'src/InspectorClientImpl.h',
                'src/InspectorFrontendClientImpl.cpp',
                'src/InspectorFrontendClientImpl.h',
                'src/NonCompositedContentHost.cpp',
                'src/NonCompositedContentHost.h',
                'src/android/WebInputEventFactory.cpp',
                'src/linux/WebFontInfo.cpp',
                'src/linux/WebFontRendering.cpp',
                'src/linux/WebFontRenderStyle.cpp',
                'src/linux/WebRenderTheme.cpp',
                'src/x11/WebScreenInfoFactory.cpp',
                'src/mac/WebInputEventFactory.mm',
                'src/mac/WebScreenInfoFactory.mm',
                'src/mac/WebSubstringUtil.mm',
                'src/LocalFileSystemChromium.cpp',
                'src/LocalizedStrings.cpp',
                'src/MediaPlayerPrivateChromium.cpp',
                'src/NotificationPresenterImpl.h',
                'src/NotificationPresenterImpl.cpp',
                'src/painting/GraphicsContextBuilder.h',
                'src/PageOverlay.cpp',
                'src/PageOverlay.h',
                'src/PageOverlayList.cpp',
                'src/PageOverlayList.h',
                'src/PlatformMessagePortChannel.cpp',
                'src/PlatformMessagePortChannel.h',
                'src/PlatformSupport.cpp',
                'src/ResourceHandle.cpp',
                'src/ScrollbarGroup.cpp',
                'src/ScrollbarGroup.h',
                'src/SharedWorkerRepository.cpp',
                'src/SocketStreamHandle.cpp',
                'src/SpeechInputClientImpl.cpp',
                'src/SpeechInputClientImpl.h',
                'src/StorageAreaProxy.cpp',
                'src/StorageAreaProxy.h',
                'src/StorageEventDispatcherChromium.cpp',
                'src/StorageEventDispatcherImpl.cpp',
                'src/StorageEventDispatcherImpl.h',
                'src/StorageInfoChromium.cpp',
                'src/StorageNamespaceProxy.cpp',
                'src/StorageNamespaceProxy.h',
                'src/UserMediaClientImpl.h',
                'src/UserMediaClientImpl.cpp',
                'src/WebTextCheckingCompletionImpl.h',
                'src/WebTextCheckingCompletionImpl.cpp',
                'src/VideoFrameChromiumImpl.cpp',
                'src/VideoFrameChromiumImpl.h',
                'src/WebAccessibilityObject.cpp',
                'src/WebAnimationControllerImpl.cpp',
                'src/WebAnimationControllerImpl.h',
                'src/WebArrayBuffer.cpp',
                'src/WebArrayBufferView.cpp',
                'src/WebAttribute.cpp',
                'src/WebAudioBus.cpp',
                'src/WebBindings.cpp',
                'src/WebBlob.cpp',
                'src/WebBlobData.cpp',
                'src/WebCache.cpp',
                'src/WebColor.cpp',
                'src/WebColorChooserClientImpl.cpp',
                'src/WebColorChooserClientImpl.h',
                'src/WebCommon.cpp',
                'src/WebCompositorImpl.cpp',
                'src/WebCompositorImpl.h',
                'src/WebCompositorInputHandlerImpl.cpp',
                'src/WebCompositorInputHandlerImpl.h',
                'src/WebContentLayer.cpp',
                'src/WebContentLayerImpl.cpp',
                'src/WebContentLayerImpl.h',
                'src/WebCrossOriginPreflightResultCache.cpp',
                'src/WebCursorInfo.cpp',
                'src/WebDOMEvent.cpp',
                'src/WebDOMEventListener.cpp',
                'src/WebDOMEventListenerPrivate.cpp',
                'src/WebDOMEventListenerPrivate.h',
                'src/WebDOMMessageEvent.cpp',
                'src/WebDOMMouseEvent.cpp',
                'src/WebDOMMutationEvent.cpp',
                'src/WebDOMStringList.cpp',
                'src/WebData.cpp',
                'src/WebDatabase.cpp',
                'src/WebDataSourceImpl.cpp',
                'src/WebDataSourceImpl.h',
                'src/WebDevToolsAgentImpl.cpp',
                'src/WebDevToolsAgentImpl.h',
                'src/WebDevToolsFrontendImpl.cpp',
                'src/WebDevToolsFrontendImpl.h',
                'src/WebDeviceOrientation.cpp',
                'src/WebDeviceOrientationClientMock.cpp',
                'src/WebDeviceOrientationController.cpp',
                'src/WebDocument.cpp',
                'src/WebDocumentType.cpp',
                'src/WebDragData.cpp',
                'src/WebElement.cpp',
                'src/WebEntities.cpp',
                'src/WebEntities.h',
                'src/WebExternalTextureLayer.cpp',
                'src/WebExternalTextureLayerImpl.cpp',
                'src/WebExternalTextureLayerImpl.h',
                'src/WebFileChooserCompletionImpl.cpp',
                'src/WebFileChooserCompletionImpl.h',
                'src/WebFileSystemCallbacksImpl.cpp',
                'src/WebFileSystemCallbacksImpl.h',
                'src/WebFloatQuad.cpp',
                'src/WebFontCache.cpp',
                'src/WebFontDescription.cpp',
                'src/WebFontImpl.cpp',
                'src/WebFontImpl.h',
                'src/WebFormControlElement.cpp',
                'src/WebFormElement.cpp',
                'src/WebFrameImpl.cpp',
                'src/WebFrameImpl.h',
                'src/WebGeolocationController.cpp',
                'src/WebGeolocationClientMock.cpp',
                'src/WebGeolocationError.cpp',
                'src/WebGeolocationPermissionRequest.cpp',
                'src/WebGeolocationPermissionRequestManager.cpp',
                'src/WebGeolocationPosition.cpp',
                'src/WebGlyphCache.cpp',
                'src/WebGraphicsContext3D.cpp',
                'src/WebHistoryItem.cpp',
                'src/WebHTTPBody.cpp',
                'src/WebHTTPLoadInfo.cpp',
                'src/WebIconLoadingCompletionImpl.cpp',
                'src/WebIconLoadingCompletionImpl.h',
                'src/WebIDBCallbacksImpl.cpp',
                'src/WebIDBCallbacksImpl.h',
                'src/WebIDBCursorImpl.cpp',
                'src/WebIDBCursorImpl.h',
                'src/WebIDBDatabaseCallbacksImpl.cpp',
                'src/WebIDBDatabaseCallbacksImpl.h',
                'src/WebIDBDatabaseError.cpp',
                'src/WebIDBDatabaseImpl.cpp',
                'src/WebIDBDatabaseImpl.h',
                'src/WebIDBFactoryImpl.cpp',
                'src/WebIDBFactoryImpl.h',
                'src/WebIDBIndexImpl.cpp',
                'src/WebIDBIndexImpl.h',
                'src/WebIDBKey.cpp',
                'src/WebIDBKeyPath.cpp',
                'src/WebIDBKeyRange.cpp',
                'src/WebIDBObjectStoreImpl.cpp',
                'src/WebIDBObjectStoreImpl.h',
                'src/WebIDBTransactionImpl.cpp',
                'src/WebIDBTransactionImpl.h',
                'src/WebIDBTransactionCallbacksImpl.cpp',
                'src/WebIDBTransactionCallbacksImpl.h',
                'src/WebImageCG.cpp',
                'src/WebImageDecoder.cpp',
                'src/WebImageSkia.cpp',
                'src/WebInputElement.cpp',
                'src/WebInputEvent.cpp',
                'src/WebInputEventConversion.cpp',
                'src/WebInputEventConversion.h',
                'src/WebIntent.cpp',
                'src/WebIntentServiceInfo.cpp',
                'src/WebKit.cpp',
                'src/WebLabelElement.cpp',
                'src/WebLayer.cpp',
                'src/WebLayerImpl.cpp',
                'src/WebLayerImpl.h',
                'src/WebLayerTreeView.cpp',
                'src/WebLayerTreeViewImpl.cpp',
                'src/WebLayerTreeViewImpl.h',
                'src/WebMediaElement.cpp',
                'src/WebMediaPlayerClientImpl.cpp',
                'src/WebMediaPlayerClientImpl.h',
                'src/WebMediaStreamDescriptor.cpp',
                'src/WebMediaStreamRegistry.cpp',
                'src/WebMediaStreamSource.cpp',
                'src/WebNamedNodeMap.cpp',
                'src/WebNetworkStateNotifier.cpp',
                'src/WebNode.cpp',
                'src/WebNodeCollection.cpp',
                'src/WebNodeList.cpp',
                'src/WebNotification.cpp',
                'src/WebOptionElement.cpp',
                'src/WebPageSerializer.cpp',
                'src/WebPageSerializerImpl.cpp',
                'src/WebPageSerializerImpl.h',
                'src/WebPasswordFormData.cpp',
                'src/WebPasswordFormUtils.cpp',
                'src/WebPasswordFormUtils.h',
                'src/WebPerformance.cpp',
                'src/WebPluginContainerImpl.h',
                'src/WebPluginContainerImpl.cpp',
                'src/WebPluginDocument.cpp',
                'src/WebPluginListBuilderImpl.cpp',
                'src/WebPluginListBuilderImpl.h',
                'src/WebPluginLoadObserver.cpp',
                'src/WebPluginLoadObserver.h',
                'src/WebPopupMenuImpl.cpp',
                'src/WebPopupMenuImpl.h',
                'src/WebRange.cpp',
                'src/WebRegularExpression.cpp',
                'src/WebRuntimeFeatures.cpp',
                'src/WebScriptController.cpp',
                'src/WebScrollbarImpl.cpp',
                'src/WebScrollbarImpl.h',
                'src/WebSearchableFormData.cpp',
                'src/WebSecurityOrigin.cpp',
                'src/WebSecurityPolicy.cpp',
                'src/WebSelectElement.cpp',
                'src/WebSerializedScriptValue.cpp',
                'src/WebSettingsImpl.cpp',
                'src/WebSettingsImpl.h',
                'src/WebSharedWorkerImpl.cpp',
                'src/WebSharedWorkerImpl.h',
                'src/WebSocket.cpp',
                'src/WebSocketImpl.cpp',
                'src/WebSocketImpl.h',
                'src/WebSpeechInputControllerMockImpl.cpp',
                'src/WebSpeechInputControllerMockImpl.h',
                'src/WebSpeechInputResult.cpp',
                'src/WebStorageAreaImpl.cpp',
                'src/WebStorageAreaImpl.h',
                'src/WebStorageEventDispatcherImpl.cpp',
                'src/WebStorageEventDispatcherImpl.h',
                'src/WebStorageNamespaceImpl.cpp',
                'src/WebStorageNamespaceImpl.h',
                'src/WebStorageQuotaCallbacksImpl.cpp',
                'src/WebStorageQuotaCallbacksImpl.h',
                'src/WebTextRun.cpp',
                'src/WebThreadSafeData.cpp',
                'src/WebURL.cpp',
                'src/WebURLLoadTiming.cpp',
                'src/WebURLRequest.cpp',
                'src/WebURLRequestPrivate.h',
                'src/WebURLResponse.cpp',
                'src/WebURLResponsePrivate.h',
                'src/WebURLError.cpp',
                'src/WebUserMediaRequest.cpp',
                'src/WebViewImpl.cpp',
                'src/WebViewImpl.h',
                'src/WebWorkerBase.cpp',
                'src/WebWorkerBase.h',
                'src/WebWorkerClientImpl.cpp',
                'src/WebWorkerClientImpl.h',
                'src/WebWorkerRunLoop.cpp',
                'src/WorkerAsyncFileSystemChromium.cpp',
                'src/WorkerAsyncFileSystemChromium.h',
                'src/WorkerAsyncFileWriterChromium.cpp',
                'src/WorkerAsyncFileWriterChromium.h',
                'src/WorkerFileSystemCallbacksBridge.cpp',
                'src/WorkerFileSystemCallbacksBridge.h',
                'src/WorkerFileWriterCallbacksBridge.cpp',
                'src/WorkerFileWriterCallbacksBridge.h',
                'src/WrappedResourceRequest.h',
                'src/WrappedResourceResponse.h',
                'src/win/WebInputEventFactory.cpp',
                'src/win/WebScreenInfoFactory.cpp',
            ],
            'conditions': [
                ['inside_chromium_build==1', {
                    'type': '<(component)',

                    'conditions': [
                        ['component=="shared_library"', {
                            'defines': [
                                'WEBKIT_DLL',
                            ],
                            'dependencies': [
                                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore_bindings',
                                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore_test_support',
                                '<(chromium_src_dir)/base/base.gyp:test_support_base',
                                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                                '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                                '<(chromium_src_dir)/testing/gmock.gyp:gmock',
                                '<(chromium_src_dir)/third_party/icu/icu.gyp:*',
                                '<(chromium_src_dir)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
                                '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
                                '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
                                '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
                                '<(chromium_src_dir)/third_party/modp_b64/modp_b64.gyp:modp_b64',
                                '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
                                '<(chromium_src_dir)/third_party/zlib/zlib.gyp:zlib',
                                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                                # We must not add webkit_support here because of cyclic dependency.
                            ],
                            'direct_dependent_settings': {
                                'defines': [
                                    'WEBKIT_DLL',
                                ],
                            },
                            'export_dependent_settings': [
                                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                            ],
                            'include_dirs': [
                                # WARNING: Do not view this particular case as a precedent for
                                # including WebCore headers in DumpRenderTree project.
                                '../../WebCore/testing/v8', # for WebCoreTestSupport.h, needed to link in window.internals code.
                            ],
                            'sources': [
                                '<@(webkit_unittest_files)',
                                'src/WebTestingSupport.cpp',
                                'public/WebTestingSupport.h',
                                'tests/WebUnitTests.cpp',   # Components test runner support.
                            ],
                            'sources!': [
                                # We should not include files depending on webkit_support.
                                # These tests depend on webkit_support and
                                # functions defined only in !WEBKIT_IMPLEMENTATION.
                                'tests/AssociatedURLLoaderTest.cpp',
                                'tests/CCLayerTreeHostTest.cpp',
                                'tests/FrameTestHelpers.cpp',
                                'tests/PopupMenuTest.cpp',
                                'tests/RenderTableCellTest.cpp',
                                'tests/WebFrameTest.cpp',
                                'tests/WebPageNewSerializerTest.cpp',
                                'tests/WebPageSerializerTest.cpp',
                                'tests/WebViewTest.cpp',
                            ],
                            'conditions': [
                                ['OS=="win" or OS=="mac"', {
                                    'dependencies': [
                                        '<(chromium_src_dir)/third_party/nss/nss.gyp:*',
                                    ],
                                }],
                                ['clang==1', {
                                    # FIXME: It would be nice to enable this in shared builds too,
                                    # but the test files have global constructors from the GTEST macro
                                    # and we pull in the test files into the webkit target in the
                                    # shared build.
                                    'cflags!': ['-Wglobal-constructors'],
                                }],
                            ],
                            'msvs_settings': {
                              'VCLinkerTool': {
                                'conditions': [
                                  ['incremental_chrome_dll==1', {
                                    'UseLibraryDependencyInputs': "true",
                                  }],
                                ],
                              },
                            },
                        }],
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
                        '<(chromium_src_dir)/build/linux/system.gyp:x11',
                    ],
                    'include_dirs': [
                        'public/x11',
                        'public/linux',
                    ],
                }, { # else: use_x11 != 1
                    'sources/': [
                        ['exclude', '/x11/'],
                        ['exclude', '/linux/'],
                    ],
                }],
                ['toolkit_uses_gtk == 1', {
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        'public/gtk',
                    ],
                }, { # else: toolkit_uses_gtk != 1
                    'sources/': [
                        ['exclude', '/gtk/'],
                    ],
                }],
                ['OS=="android"', {
                    'include_dirs': [
                        'public/android',
                    ],
                }, { # else: OS!="android"
                    'sources/': [
                        ['exclude', '/android/'],
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        'public/mac',
                    ],
                    'conditions': [
                        ['use_skia==0', {
                            'sources/': [
                                ['exclude', 'Skia\\.cpp$'],
                            ],
                        },{ # use_skia
                            'sources/': [
                                ['exclude', 'CG\\.cpp$'],
                            ],
                        }],
                    ],
                }, { # else: OS!="mac"
                    'sources/': [
                        ['exclude', '/mac/'],
                        ['exclude', 'CG\\.cpp$'],
                    ],
                }],
                ['OS=="win"', {
                    'include_dirs': [
                        'public/win',
                    ],
                }, { # else: OS!="win"
                    'sources/': [['exclude', '/win/']],
                    'variables': {
                        # FIXME: Turn on warnings on Windows.
                        'chromium_code': 1,
                    }
                }],
                ['"ENABLE_WEBGL=1" in feature_defines', {
                    'conditions': [
                        ['OS=="mac"', {
                            'link_settings': {
                                'libraries': [
                                    '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                                ],
                            },
                        }],
                    ],
                }],
                ['clang==1', {
                    'cflags': ['-Wglobal-constructors'],
                    'xcode_settings': {
                        'WARNING_CFLAGS': ['-Wglobal-constructors'],
                    },
                }],
            ],
        },
        {
            'target_name': 'inspector_resources',
            'type': 'none',
            'dependencies': [
                'devtools_html',
                '../../WebCore/WebCore.gyp/WebCore.gyp:inspector_protocol_sources',
            ],
            'conditions': [
                ['debug_devtools==0', {
                    'dependencies': ['concatenated_devtools_js',
                                     'concatenated_heap_snapshot_worker_js',
                                     'concatenated_script_formatter_worker_js',
                                     'concatenated_devtools_css'],
                }],
            ],
            'copies': [
                {
                    'destination': '<(PRODUCT_DIR)/resources/inspector',
                    'files': [
                        '<@(devtools_files)',
                        '<@(webinspector_files)',
                        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendStub.js',
                    ],
                    'conditions': [
                        ['debug_devtools==0', {
                            'files/': [['exclude', '\\.(js|css|html)$']],
                        }],
                    ],
                },
                {
                    'destination': '<(PRODUCT_DIR)/resources/inspector/UglifyJS',
                    'files': [
                        '<@(webinspector_uglifyjs_files)',
                    ],
                    'conditions': [
                        ['debug_devtools==0', {
                            'files/': [['exclude', '\\.(js|css|html)$']],
                        }],
                    ],
                },
                {
                    'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
                    'files': [
                        '<@(webinspector_image_files)',
                        '<@(devtools_image_files)',
                    ],
               },
            ],
        },
        {
            'target_name': 'devtools_html',
            'type': 'none',
            'sources': ['<(PRODUCT_DIR)/resources/inspector/devtools.html'],
            'actions': [{
                'action_name': 'devtools_html',
                'script_name': 'scripts/generate_devtools_html.py',
                'input_page': '../../WebCore/inspector/front-end/inspector.html',
                'inputs': [
                    '<@(_script_name)',
                    '<@(_input_page)',
                    '<@(devtools_files)',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/devtools.html'],
                'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_outputs)', '<@(debug_devtools)', '<@(devtools_files)'],
            }],
        },
        {
            'target_name': 'devtools_extension_api',
            'type': 'none',
            'actions': [{
                'action_name': 'devtools_html',
                'script_name': 'scripts/generate_devtools_extension_api.py',
                'inputs': [
                    '<@(_script_name)',
                    '<@(devtools_extension_api_files)',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/devtools_extension_api.js'],
                'action': ['python', '<@(_script_name)', '<@(_outputs)', '<@(devtools_extension_api_files)'],
            }],
        },
        {
            'target_name': 'generate_devtools_grd',
            'type': 'none',
            'dependencies': [
                'devtools_html',
                'devtools_extension_api'
            ],
            'conditions': [
                ['debug_devtools==0', {
                    'dependencies': ['concatenated_devtools_js',
                                     'concatenated_heap_snapshot_worker_js',
                                     'concatenated_script_formatter_worker_js',
                                     'concatenated_devtools_css'],
                },{
                    # If we're not concatenating devtools files, we want to
                    # run after the original files have been copied to
                    # <(PRODUCT_DIR)/resources/inspector.
                    'dependencies': ['inspector_resources'],
                }],
            ],
            'actions': [{
                'action_name': 'generate_devtools_grd',
                'script_name': 'scripts/generate_devtools_grd.py',
                'input_pages': [
                    '<(PRODUCT_DIR)/resources/inspector/devtools.html',
                    '<(PRODUCT_DIR)/resources/inspector/DevTools.js',
                    '<(PRODUCT_DIR)/resources/inspector/HeapSnapshotWorker.js',
                    '<(PRODUCT_DIR)/resources/inspector/ScriptFormatterWorker.js',
                    '<(PRODUCT_DIR)/resources/inspector/devTools.css',
                    '<(PRODUCT_DIR)/resources/inspector/devtools_extension_api.js',
                    '<@(webinspector_standalone_css_files)',
                ],
                'images': [
                    '<@(webinspector_image_files)',
                    '<@(devtools_image_files)',
                ],
                'inputs': [
                    '<@(_script_name)',
                    '<@(_input_pages)',
                    '<@(_images)',
                ],
                'search_path': [
                    '../../WebCore/inspector/front-end/Images',
                    'src/js/Images',
                ],
                'outputs': ['<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd'],
                'action': ['python', '<@(_script_name)', '<@(_input_pages)', '--images', '<@(_search_path)', '--output', '<@(_outputs)'],
            }],
        },
        {
            'target_name': 'generate_devtools_zip',
            'type': 'none',
            'dependencies': [
                '../../WebCore/WebCore.gyp/WebCore.gyp:inspector_protocol_sources',
            ],
            'actions': [{
                'action_name': 'generate_devtools_zip',
                'script_name': 'scripts/generate_devtools_zip.py',
                'inspector_html': '../../WebCore/inspector/front-end/inspector.html',
                'workers_files': [
                    '../../WebCore/inspector/front-end/HeapSnapshotWorker.js',
                    '../../WebCore/inspector/front-end/JavaScriptFormatter.js',
                    '../../WebCore/inspector/front-end/ScriptFormatterWorker.js',
                    '<@(webinspector_uglifyjs_files)'
                ],
                'inputs': [
                    '<@(_script_name)',
                    'scripts/generate_devtools_html.py',
                    'scripts/generate_devtools_extension_api.py',
                    '<@(_inspector_html)',
                    '<@(devtools_files)',
                    '<@(webinspector_files)',
                    '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendStub.js',
                    '<@(_workers_files)',
                    '<@(webinspector_image_files)',
                    '<@(devtools_image_files)',
                    '<@(devtools_extension_api_files)',
                ],
                'search_path': [
                    '../../WebCore/inspector/front-end',
                    'src/js',
                ],
                'js_search_path': [
                    '<(SHARED_INTERMEDIATE_DIR)/webcore',
                ],
                'outputs': ['<(PRODUCT_DIR)/devtools_frontend.zip'],
                'action': ['python', '<@(_script_name)', '<@(_inspector_html)',
                                     '--devtools-files', '<@(devtools_files)',
                                     '--workers-files', '<@(_workers_files)',
                                     '--extension-api-files', '<@(devtools_extension_api_files)',
                                     '--search-path', '<@(_search_path)',
                                     '--js-search-path', '<@(_js_search_path)',
                                     '--output', '<@(_outputs)'],
            }],
        },
    ], # targets
    'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="android" and gcc_version==46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="mac"', {
            'targets': [
                {
                    'target_name': 'copy_mesa',
                    'type': 'none',
                    'dependencies': ['<(chromium_src_dir)/third_party/mesa/mesa.gyp:osmesa'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/DumpRenderTree.app/Contents/MacOS/',
                        'files': ['<(PRODUCT_DIR)/osmesa.so'],
                    }],
                },
            ],
        }],
        ['debug_devtools==0', {
            'targets': [
                {
                    'target_name': 'concatenated_devtools_js',
                    'type': 'none',
                    'dependencies': [
                        'devtools_html',
                        '../../WebCore/WebCore.gyp/WebCore.gyp:inspector_protocol_sources'
                    ],
                    'actions': [{
                        'action_name': 'concatenate_devtools_js',
                        'script_name': 'scripts/concatenate_js_files.py',
                        'input_page': '../../WebCore/inspector/front-end/inspector.html',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_page)',
                            '<@(webinspector_files)',
                            '<@(devtools_files)',
                            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendStub.js'
                        ],
                        'search_path': [
                            '../../WebCore/inspector/front-end',
                            'src/js',
                            '<(SHARED_INTERMEDIATE_DIR)/webcore',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/DevTools.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_search_path)', '<@(_outputs)'],
                    }],
                },
                {
                    'target_name': 'concatenated_heap_snapshot_worker_js',
                    'type': 'none',
                    'actions': [{
                        'action_name': 'concatenate_heap_snapshot_worker_js',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': '../../WebCore/inspector/front-end/HeapSnapshotWorker.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_file)',
                            '../../WebCore/inspector/front-end/BinarySearch.js',
                            '../../WebCore/inspector/front-end/HeapSnapshot.js',
                            '../../WebCore/inspector/front-end/HeapSnapshotWorkerDispatcher.js',
                            '../../WebCore/inspector/front-end/PartialQuickSort.js',
                        ],
                        'search_path': '../../WebCore/inspector/front-end',
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/HeapSnapshotWorker.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_search_path)', '<@(_outputs)'],
                    }],
                },
                {
                    'target_name': 'concatenated_script_formatter_worker_js',
                    'type': 'none',
                    'actions': [{
                        'action_name': 'concatenate_script_formatter_worker_js',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': '../../WebCore/inspector/front-end/ScriptFormatterWorker.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_file)',
                            '<@(webinspector_uglifyjs_files)'
                        ],
                        'search_path': '../../WebCore/inspector/front-end',
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/ScriptFormatterWorker.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_search_path)', '<@(_outputs)'],
                    }],
                },
                {
                    'target_name': 'concatenated_devtools_css',
                    'type': 'none',
                    'dependencies': [
                        'devtools_html'
                    ],
                    'actions': [{
                        'action_name': 'concatenate_devtools_css',
                        'script_name': 'scripts/concatenate_css_files.py',
                        'input_page': '../../WebCore/inspector/front-end/inspector.html',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_page)',
                            '<@(webinspector_files)',
                            '<@(devtools_files)'
                        ],
                        'search_path': [
                            '../../WebCore/inspector/front-end',
                            'src/js',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/devTools.css'],
                        'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_search_path)', '<@(_outputs)'],
                    }],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/resources/inspector',
                        'files': [
                            '<@(webinspector_standalone_css_files)',
                        ],
                    }],
                },
            ],
        }],
        # FIXME: Delete this whole block once chromium's build/common.gypi
        # is setting this flag to 0 by default. See
        # https://bugs.webkit.org/show_bug.cgi?id=68463.
        ['build_webkit_exes_from_webkit_gyp==1', {
            'includes': [
                '../../../Tools/DumpRenderTree/DumpRenderTree.gypi',
                '../../../Tools/TestWebKitAPI/TestWebKitAPI.gypi',
            ],
            'targets': [
                {
                    'target_name': 'webkit_unit_tests',
                    'type': 'executable',
                    'dependencies': [
                        'webkit',
                        '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                        '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                        '<(chromium_src_dir)/testing/gmock.gyp:gmock',
                        '<(chromium_src_dir)/base/base.gyp:base',
                        '<(chromium_src_dir)/base/base.gyp:base_i18n',
                        '<(chromium_src_dir)/base/base.gyp:test_support_base',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_user_agent',
                    ],
                    'sources': [
                        'tests/RunAllTests.cpp',
                    ],
                    'include_dirs': [
                        'public',
                        'src',
                    ],
                    'conditions': [
                        ['inside_chromium_build==1 and component=="shared_library"', {
                            'defines': [
                                'WEBKIT_DLL_UNITTEST',
                            ],
                        }, {
                            'sources': [
                                '<@(webkit_unittest_files)',
                            ],
                            'conditions': [
                                ['toolkit_uses_gtk == 1', {
                                    'include_dirs': [
                                        'public/gtk',
                                    ],
                                    'variables': {
                                    # FIXME: Enable warnings on other platforms.
                                    'chromium_code': 1,
                                    },
                                }],
                            ],
                        }],
                        ['inside_chromium_build==1 and OS=="win" and component!="shared_library"', {
                            'configurations': {
                                'Debug_Base': {
                                    'msvs_settings': {
                                        'VCLinkerTool': {
                                            'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                        },
                                    },
                                },
                            },
                        }],
                    ],
                },
                {
                    'target_name': 'ImageDiff',
                    'type': 'executable',
                    'dependencies': [
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support_gfx',
                    ],
                    'include_dirs': [
                        '../../JavaScriptCore',
                        '<(DEPTH)',
                    ],
                    'sources': [
                        '../../../Tools/DumpRenderTree/chromium/ImageDiff.cpp',
                    ],
                    'conditions': [
                        ['OS=="android"', {
                            'toolsets': ['host'],
                        }],
                    ],
                },
                {
                    'target_name': 'DumpRenderTree',
                    'type': 'executable',
                    'mac_bundle': 1,
                    'dependencies': [
                        'inspector_resources',
                        'webkit',
                        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf_config',
                        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                        '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
                        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:blob',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_user_agent',
                    ],
                    'include_dirs': [
                        '<(chromium_src_dir)',
                        'public',
                        '../../JavaScriptCore',
                        '../../JavaScriptCore/wtf', # wtf/text/*.h refers headers in wtf/ without wtf/.
                        '<(DEPTH)',
                    ],
                    'defines': [
                        # Technically not a unit test but require functions available only to
                        # unit tests.
                        'UNIT_TEST',
                    ],
                    'sources': [
                        '<@(drt_files)',
                    ],
                    'conditions': [
                        ['OS=="win"', {
                            'dependencies': [
                                'LayoutTestHelper',
                                '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:libEGL',
                                '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:libGLESv2',
                            ],

                            'resource_include_dirs': ['<(SHARED_INTERMEDIATE_DIR)/webkit'],
                            'sources': [
                                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
                                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
                            ],
                            'conditions': [
                                ['inside_chromium_build==1', {
                                    'configurations': {
                                        'Debug_Base': {
                                            'msvs_settings': {
                                                'VCLinkerTool': {
                                                    'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                                },
                                            },
                                        },
                                    },
                                }],
                            ],
                            'copies': [{
                                'destination': '<(PRODUCT_DIR)',
                                'files': ['<(ahem_path)'],
                            }],
                        },{ # OS!="win"
                            'sources/': [
                                ['exclude', 'Win\\.cpp$'],
                            ],
                            'actions': [
                                {
                                    'action_name': 'repack_locale',
                                    'variables': {
                                        'repack_path': '<(chromium_src_dir)/tools/grit/grit/format/repack.py',
                                        'pak_inputs': [
                                            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                                            '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                                            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                                            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                                            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                                    ]},
                                    'inputs': [
                                        '<(repack_path)',
                                        '<@(pak_inputs)',
                                    ],
                                    'outputs': [
                                        '<(INTERMEDIATE_DIR)/repack/DumpRenderTree.pak',
                                    ],
                                    'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
                                    'process_outputs_as_mac_bundle_resources': 1,
                                },
                            ], # actions
                        }],
                        ['OS=="mac"', {
                            'dependencies': [
                                'copy_mesa',
                                'LayoutTestHelper',
                            ],
                            'mac_bundle_resources': [
                                '<(ahem_path)',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher100.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher200.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher300.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher400.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher500.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher600.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher700.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher800.ttf',
                                '../../../Tools/DumpRenderTree/fonts/WebKitWeightWatcher900.ttf',
                                '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
                            ],
                        },{ # OS!="mac"
                            'sources/': [
                                # .mm is already excluded by common.gypi
                                ['exclude', 'Mac\\.cpp$'],
                            ],
                        }],
                        ['use_x11 == 1', {
                            'dependencies': [
                                '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
                            ],
                            'copies': [{
                                'destination': '<(PRODUCT_DIR)',
                                'files': [
                                    '<(ahem_path)',
                                    '../../../Tools/DumpRenderTree/chromium/fonts.conf',
                                    '<(INTERMEDIATE_DIR)/repack/DumpRenderTree.pak',
                                ]
                            }],
                            'variables': {
                                # FIXME: Enable warnings on other platforms.
                                'chromium_code': 1,
                            },
                            'conditions': [
                                ['linux_use_tcmalloc == 1', {
                                    'dependencies': [
                                        '<(chromium_src_dir)/base/allocator/allocator.gyp:allocator',
                                    ],
                                }],
                            ],
                        },{ # use_x11 != 1
                            'sources/': [
                                ['exclude', 'Linux\\.cpp$']
                            ]
                        }],
                        ['toolkit_uses_gtk == 1', {
                            'defines': [
                                'WTF_USE_GTK=1',
                            ],
                            'dependencies': [
                                '<(chromium_src_dir)/build/linux/system.gyp:gtk',
                            ],
                            'include_dirs': [
                                'public/gtk',
                            ],
                        },{ # toolkit_uses_gtk != 1
                            'sources/': [
                                ['exclude', 'Gtk\\.cpp$']
                            ]
                        }],
                        ['OS=="android"', {
                            'sources/': [
                                ['include', 'chromium/TestShellLinux\\.cpp$'],
                            ],
                            'dependencies': [
                                'ImageDiff#host',
                            ],
                        },{ # OS!="android"
                            'dependencies': [
                                'ImageDiff',
                                'copy_TestNetscapePlugIn',
                                '<(chromium_src_dir)/third_party/mesa/mesa.gyp:osmesa',
                            ],
                        }],
                        ['inside_chromium_build==1 and component=="shared_library"', {
                            'sources': [
                                'src/ChromiumCurrentTime.cpp',
                                'src/ChromiumThreading.cpp',
                            ],
                            'include_dirs': [
                                'public',
                            ],
                            'dependencies': [
                                '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
                                '../../WTF/WTF.gyp/WTF.gyp:newwtf',
                            ],
                        }],
                        ['inside_chromium_build==0', {
                            'dependencies': [
                                '<(chromium_src_dir)/webkit/support/setup_third_party.gyp:third_party_headers',
                            ]
                        }],
                        ['inside_chromium_build==0 or component!="shared_library"', {
                            'dependencies': [
                                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore_test_support',
                            ],
                            'include_dirs': [
                                # WARNING: Do not view this particular case as a precedent for
                                # including WebCore headers in DumpRenderTree project.
                                '../../WebCore/testing/v8', # for WebCoreTestSupport.h, needed to link in window.internals code.
                            ],
                            'sources': [
                                'src/WebTestingSupport.cpp',
                                'public/WebTestingSupport.h',
                            ],
                        }],
                    ],
                },
                {
                    'target_name': 'TestNetscapePlugIn',
                    'type': 'loadable_module',
                    'sources': [ '<@(test_plugin_files)' ],
                    'dependencies': [
                        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
                    ],
                    'include_dirs': [
                        '<(chromium_src_dir)',
                        '../../../Tools/DumpRenderTree/TestNetscapePlugIn',
                        '../../../Tools/DumpRenderTree/chromium/TestNetscapePlugIn/ForwardingHeaders',
                    ],
                    'conditions': [
                        ['OS=="mac"', {
                            'mac_bundle': 1,
                            'product_extension': 'plugin',
                            'link_settings': {
                                'libraries': [
                                    '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                                    '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                                    '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
                                ]
                            },
                            'xcode_settings': {
                                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
                                'INFOPLIST_FILE': '../../../Tools/DumpRenderTree/TestNetscapePlugIn/mac/Info.plist',
                            },
                        }],
                        ['os_posix == 1 and OS != "mac"', {
                            'cflags': [
                                '-fvisibility=default',
                            ],
                        }],
                        ['OS=="android"', {
                            'ldflags!': [
                                '-nostdlib',
                            ],
                        }],
                        ['OS=="win"', {
                            'defines': [
                                # This seems like a hack, but this is what Safari Win does.
                                'snprintf=_snprintf',
                            ],
                            'sources': [
                                '../../../Tools/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.def',
                                '../../../Tools/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.rc',
                            ],
                            # The .rc file requires that the name of the dll is npTestNetscapePlugin.dll.
                            'product_name': 'npTestNetscapePlugin',
                        }],
                    ],
                },
                {
                    'target_name': 'copy_TestNetscapePlugIn',
                    'type': 'none',
                    'dependencies': [
                        'TestNetscapePlugIn',
                    ],
                    'conditions': [
                        ['OS=="win"', {
                            'copies': [{
                                'destination': '<(PRODUCT_DIR)/plugins',
                                'files': ['<(PRODUCT_DIR)/npTestNetscapePlugIn.dll'],
                            }],
                        }],
                        ['OS=="mac"', {
                            'dependencies': ['TestNetscapePlugIn'],
                            'copies': [{
                                'destination': '<(PRODUCT_DIR)/plugins/',
                                'files': ['<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/'],
                            }],
                        }],
                        ['os_posix == 1 and OS != "mac"', {
                            'copies': [{
                                'destination': '<(PRODUCT_DIR)/plugins',
                                'files': ['<(PRODUCT_DIR)/libTestNetscapePlugIn.so'],
                            }],
                        }],
                    ],
                },
                {
                    'target_name': 'TestWebKitAPI',
                    'type': 'executable',
                    'dependencies': [
                        'webkit',
                        '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                        '<(chromium_src_dir)/base/base.gyp:test_support_base',
                        '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                        '<(chromium_src_dir)/testing/gmock.gyp:gmock',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                    ],
                    'include_dirs': [
                        '../../../Tools/TestWebKitAPI',
                        # Needed by tests/RunAllTests.cpp, as well as ChromiumCurrentTime.cpp and
                        # ChromiumThreading.cpp in chromium shared library configuration.
                        'public',
                    ],
                    'sources': [
                        # Reuse the same testing driver of Chromium's webkit_unit_tests.
                        'tests/RunAllTests.cpp',
                        '<@(TestWebKitAPI_files)',
                    ],
                    'conditions': [
                        ['inside_chromium_build==1 and component=="shared_library"', {
                            'sources': [
                                # To satisfy linking of WTF::currentTime() etc. in shared library configuration,
                                # as the symbols are not exported from the DLLs.
                                'src/ChromiumCurrentTime.cpp',
                                'src/ChromiumThreading.cpp',
                            ],
                        }],
                    ],
                },
            ], # targets
            'conditions': [
                ['OS=="win"', {
                    'targets': [{
                        'target_name': 'LayoutTestHelper',
                        'type': 'executable',
                        'sources': ['../../../Tools/DumpRenderTree/chromium/LayoutTestHelperWin.cpp'],
                    }],
                }],
                ['OS=="mac"', {
                    'targets': [{
                        'target_name': 'LayoutTestHelper',
                        'type': 'executable',
                        'sources': ['../../../Tools/DumpRenderTree/chromium/LayoutTestHelper.mm'],
                        'link_settings': {
                            'libraries': [
                                '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                            ],
                        },
                    }],
                }],
            ],
        }],
    ], # conditions
}

#
# Copyright (C) 2010 Google Inc. All rights reserved.
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
        '../../WebKitTools/DumpRenderTree/DumpRenderTree.gypi',
        'WebKit.gypi',
        'features.gypi',
    ],
    'variables': {
        'webkit_target_type': 'static_library',
        'conditions': [
            # Location of the chromium src directory and target type is different
            # if webkit is built inside chromium or as standalone project.
            ['inside_chromium_build==0', {
                # Webkit is being built outside of the full chromium project.
                # e.g. via build-webkit --chromium
                'chromium_src_dir': '../../WebKit/chromium',

                # List of DevTools source files, ordered by dependencies. It is used both
                # for copying them to resource dir, and for generating 'devtools.html' file.
                'devtools_files': [
                    '<@(devtools_css_files)',
                    '<@(devtools_js_files)',
                ],
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '../../../..',

                'devtools_files': [
                    '<@(devtools_css_files)',
                    '<@(devtools_js_files)',
                ],
            }],
        ],
        'ahem_path': '../../WebKitTools/DumpRenderTree/qt/fonts/AHEM____.TTF',

        # If debug_devtools is set to 1, JavaScript files for DevTools are
        # stored as is. Otherwise, a concatenated file is stored.
        'debug_devtools%': 0,
    },
    'targets': [
        {
            'target_name': 'webkit',
            'msvs_guid': '5ECEC9E5-8F23-47B6-93E0-C3B328B3BE65',
            'dependencies': [
                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                '<(chromium_src_dir)/app/app.gyp:app_base', # For GLContext
                '<(chromium_src_dir)/skia/skia.gyp:skia',
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
            ],
            'export_dependent_settings': [
                '<(chromium_src_dir)/skia/skia.gyp:skia',
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
            ],
            'include_dirs': [
                'public',
                'src',
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION=1',
            ],
            'sources': [
                'public/gtk/WebInputEventFactory.h',
                'public/linux/WebFontRendering.h',
                'public/linux/WebFontRenderStyle.h',
                'public/linux/WebRenderTheme.h',
                'public/x11/WebScreenInfoFactory.h',
                'public/mac/WebInputEventFactory.h',
                'public/mac/WebSandboxSupport.h',
                'public/mac/WebScreenInfoFactory.h',
                'public/WebAccessibilityCache.h',
                'public/WebAccessibilityObject.h',
                'public/WebAccessibilityRole.h',
                'public/WebAnimationController.h',
                'public/WebApplicationCacheHost.h',
                'public/WebApplicationCacheHostClient.h',
                'public/WebAttribute.h',
                'public/WebBindings.h',
                'public/WebBlobData.h',
                'public/WebBlobRegistry.h',
                'public/WebBlobStorageData.h',
                'public/WebCache.h',
                'public/WebCanvas.h',
                'public/WebClipboard.h',
                'public/WebColor.h',
                'public/WebColorName.h',
                'public/WebCommon.h',
                'public/WebCommonWorkerClient.h',
                'public/WebCompositionUnderline.h',
                'public/WebConsoleMessage.h',
                'public/WebContextMenuData.h',
                'public/WebCookie.h',
                'public/WebCookieJar.h',
                'public/WebCrossOriginPreflightResultCache.h',
                'public/WebCString.h',
                'public/WebCursorInfo.h',
                'public/WebDOMEvent.h',
                'public/WebDOMEventListener.h',
                'public/WebDOMMouseEvent.h',
                'public/WebDOMMutationEvent.h',
                'public/WebDOMStringList.h',
                'public/WebData.h',
                'public/WebDatabase.h',
                'public/WebDatabaseObserver.h',
                'public/WebDataSource.h',
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
                'public/WebDragData.h',
                'public/WebEditingAction.h',
                'public/WebElement.h',
                'public/WebFileChooserCompletion.h',
                'public/WebFileChooserParams.h',
                'public/WebFileError.h',
                'public/WebFileInfo.h',
                'public/WebFileSystem.h',
                'public/WebFileSystemCallbacks.h',
                'public/WebFileSystemEntry.h',
                'public/WebFileUtilities.h',
                'public/WebFindOptions.h',
                'public/WebFloatPoint.h',
                'public/WebFloatRect.h',
                'public/WebFont.h',
                'public/WebFontDescription.h',
                'public/WebFrame.h',
                'public/WebFrameClient.h',
                'public/WebFontCache.h',
                'public/WebFormControlElement.h',
                'public/WebFormElement.h',
                'public/WebGeolocationService.h',
                'public/WebGeolocationServiceBridge.h',
                'public/WebGeolocationServiceMock.h',
                'public/WebGlyphCache.h',
                'public/WebGLES2Context.h',
                'public/WebGraphicsContext3D.h',
                'public/WebHistoryItem.h',
                'public/WebHTTPBody.h',
                'public/WebImage.h',
                'public/WebImageDecoder.h',
                'public/WebIDBCallbacks.h',
                'public/WebIDBCursor.h',
                'public/WebIDBDatabase.h',
                'public/WebIDBDatabaseError.h',
                'public/WebIDBFactory.h',
                'public/WebIDBKeyRange.h',
                'public/WebIDBIndex.h',
                'public/WebIDBKey.h',
                'public/WebIDBKeyPath.h',
                'public/WebIDBObjectStore.h',
                'public/WebIDBTransaction.h',
                'public/WebIDBTransactionCallbacks.h',
                'public/WebInputElement.h',
                'public/WebInputEvent.h',
                'public/WebKit.h',
                'public/WebKitClient.h',
                'public/WebLabelElement.h',
                'public/WebLocalizedString.h',
                'public/WebMediaElement.h',
                'public/WebMediaPlayer.h',
                'public/WebMediaPlayerAction.h',
                'public/WebMediaPlayerClient.h',
                'public/WebMenuItemInfo.h',
                'public/WebMessagePortChannel.h',
                'public/WebMessagePortChannelClient.h',
                'public/WebMimeRegistry.h',
                'public/WebNamedNodeMap.h',
                'public/WebNavigationType.h',
                'public/WebNode.h',
                'public/WebNodeCollection.h',
                'public/WebNodeList.h',
                'public/WebNonCopyable.h',
                'public/WebNotification.h',
                'public/WebNotificationPresenter.h',
                'public/WebNotificationPermissionCallback.h',
                'public/WebOptionElement.h',
                'public/WebPageSerializer.h',
                'public/WebPageSerializerClient.h',
                'public/WebPasswordAutocompleteListener.h',
                'public/WebPasswordFormData.h',
                'public/WebPlugin.h',
                'public/WebPluginContainer.h',
                'public/WebPluginDocument.h',
                'public/WebPluginListBuilder.h',
                'public/WebPoint.h',
                'public/WebPopupMenu.h',
                'public/WebPopupMenuInfo.h',
                'public/WebPopupType.h',
                'public/WebPrivatePtr.h',
                'public/WebPrivateOwnPtr.h',
                'public/WebRange.h',
                'public/WebRect.h',
                'public/WebRegularExpression.h',
                'public/WebRuntimeFeatures.h',
                'public/WebScrollbar.h',
                'public/WebScrollbarClient.h',
                'public/WebScreenInfo.h',
                'public/WebScriptController.h',
                'public/WebScriptSource.h',
                'public/WebSearchableFormData.h',
                'public/WebSecurityOrigin.h',
                'public/WebSecurityPolicy.h',
                'public/WebSelectElement.h',
                'public/WebSerializedScriptValue.h',
                'public/WebSettings.h',
                'public/WebSharedWorker.h',
                'public/WebSharedWorkerRepository.h',
                'public/WebSize.h',
                'public/WebSocketStreamError.h',
                'public/WebSocketStreamHandle.h',
                'public/WebSocketStreamHandleClient.h',
                'public/WebSpeechInputController.h',
                'public/WebSpeechInputControllerMock.h',
                'public/WebSpeechInputListener.h',
                'public/WebStorageArea.h',
                'public/WebStorageEventDispatcher.h',
                'public/WebStorageNamespace.h',
                'public/WebString.h',
                'public/WebTextAffinity.h',
                'public/WebTextCaseSensitivity.h',
                'public/WebTextDirection.h',
                'public/WebTextInputType.h',
                'public/WebTextRun.h',
                'public/WebThemeEngine.h',
                'public/WebURL.h',
                'public/WebURLError.h',
                'public/WebURLLoader.h',
                'public/WebURLLoadTiming.h',
                'public/WebURLLoaderClient.h',
                'public/WebURLRequest.h',
                'public/WebURLResponse.h',
                'public/WebVector.h',
                'public/WebView.h',
                'public/WebViewClient.h',
                'public/WebWidget.h',
                'public/WebWidgetClient.h',
                'public/WebWorker.h',
                'public/WebWorkerClient.h',
                'public/win/WebInputEventFactory.h',
                'public/win/WebSandboxSupport.h',
                'public/win/WebScreenInfoFactory.h',
                'public/win/WebScreenInfoFactory.h',
                'src/ApplicationCacheHost.cpp',
                'src/ApplicationCacheHostInternal.h',
                'src/AssertMatchingEnums.cpp',
                'src/AsyncFileSystemChromium.cpp',
                'src/AsyncFileSystemChromium.h',
                'src/AutoFillPopupMenuClient.cpp',
                'src/AutoFillPopupMenuClient.h',
                'src/BackForwardListClientImpl.cpp',
                'src/BackForwardListClientImpl.h',
                'src/BlobRegistryProxy.cpp',
                'src/BlobRegistryProxy.h',
                'src/BoundObject.cpp',
                'src/BoundObject.h',
                'src/ChromeClientImpl.cpp',
                'src/ChromeClientImpl.h',
                'src/ChromiumBridge.cpp',
                'src/ChromiumCurrentTime.cpp',
                'src/ChromiumThreading.cpp',
                'src/CompositionUnderlineBuilder.h',
                'src/CompositionUnderlineVectorBuilder.cpp',
                'src/CompositionUnderlineVectorBuilder.h',
                'src/ContextMenuClientImpl.cpp',
                'src/ContextMenuClientImpl.h',
                'src/DatabaseObserver.cpp',
                'src/DebuggerAgentImpl.cpp',
                'src/DebuggerAgentImpl.h',
                'src/DebuggerAgentManager.cpp',
                'src/DebuggerAgentManager.h',
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
                'src/FrameLoaderClientImpl.cpp',
                'src/FrameLoaderClientImpl.h',
                'src/FrameNetworkingContextImpl.h',
                'src/GLES2Context.cpp',
                'src/GLES2ContextInternal.cpp',
                'src/GLES2ContextInternal.h',
                'src/GraphicsContext3D.cpp',
                'src/gtk/WebFontInfo.cpp',
                'src/gtk/WebFontInfo.h',
                'src/gtk/WebInputEventFactory.cpp',
                'src/IDBCallbacksProxy.cpp',
                'src/IDBCallbacksProxy.h',
                'src/IDBCursorBackendProxy.cpp',
                'src/IDBCursorBackendProxy.h',
                'src/IDBDatabaseProxy.cpp',
                'src/IDBDatabaseProxy.h',
                'src/IDBFactoryBackendProxy.cpp',
                'src/IDBFactoryBackendProxy.h',
                'src/IDBIndexBackendProxy.cpp',
                'src/IDBIndexBackendProxy.h',
                'src/IDBObjectStoreProxy.cpp',
                'src/IDBObjectStoreProxy.h',
                'src/IDBTransactionBackendProxy.cpp',
                'src/IDBTransactionBackendProxy.h',
                'src/IDBTransactionCallbacksProxy.cpp',
                'src/IDBTransactionCallbacksProxy.h',
                'src/InspectorClientImpl.cpp',
                'src/InspectorClientImpl.h',
                'src/InspectorFrontendClientImpl.cpp',
                'src/InspectorFrontendClientImpl.h',
                'src/linux/WebFontRendering.cpp',
                'src/linux/WebFontRenderStyle.cpp',
                'src/linux/WebRenderTheme.cpp',
                'src/x11/WebScreenInfoFactory.cpp',
                'src/mac/WebInputEventFactory.mm',
                'src/mac/WebScreenInfoFactory.mm',
                'src/LocalFileSystemChromium.cpp',
                'src/LocalizedStrings.cpp',
                'src/MediaPlayerPrivateChromium.cpp',
                'src/NotificationPresenterImpl.h',
                'src/NotificationPresenterImpl.cpp',
                'src/PlatformMessagePortChannel.cpp',
                'src/PlatformMessagePortChannel.h',
                'src/ResourceHandle.cpp',
                'src/SharedWorkerRepository.cpp',
                'src/SocketStreamHandle.cpp',
                'src/SpeechInputClientImpl.cpp',
                'src/SpeechInputClientImpl.h',
                'src/StorageAreaProxy.cpp',
                'src/StorageAreaProxy.h',
                'src/StorageEventDispatcherChromium.cpp',
                'src/StorageEventDispatcherImpl.cpp',
                'src/StorageEventDispatcherImpl.h',
                'src/StorageNamespaceProxy.cpp',
                'src/StorageNamespaceProxy.h',
                'src/TemporaryGlue.h',
                'src/VideoFrameChromiumImpl.cpp',
                'src/VideoFrameChromiumImpl.h',
                'src/WebAccessibilityCache.cpp',
                'src/WebAccessibilityCacheImpl.cpp',
                'src/WebAccessibilityCacheImpl.h',
                'src/WebAccessibilityObject.cpp',
                'src/WebAnimationControllerImpl.cpp',
                'src/WebAnimationControllerImpl.h',
                'src/WebAttribute.cpp',
                'src/WebBindings.cpp',
                'src/WebBlobData.cpp',
                'src/WebBlobStorageData.cpp',
                'src/WebCache.cpp',
                'src/WebColor.cpp',
                'src/WebCommon.cpp',
                'src/WebCrossOriginPreflightResultCache.cpp',
                'src/WebCString.cpp',
                'src/WebCursorInfo.cpp',
                'src/WebDOMEvent.cpp',
                'src/WebDOMEventListener.cpp',
                'src/WebDOMEventListenerPrivate.cpp',
                'src/WebDOMEventListenerPrivate.h',
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
                'src/WebFileChooserCompletionImpl.cpp',
                'src/WebFileChooserCompletionImpl.h',
                'src/WebFileSystemCallbacksImpl.cpp',
                'src/WebFileSystemCallbacksImpl.h',
                'src/WebFontCache.cpp',
                'src/WebFontDescription.cpp',
                'src/WebFontImpl.cpp',
                'src/WebFontImpl.h',
                'src/WebFormControlElement.cpp',
                'src/WebFormElement.cpp',
                'src/WebFrameImpl.cpp',
                'src/WebFrameImpl.h',
                'src/WebGeolocationServiceBridgeImpl.cpp',
                'src/WebGeolocationServiceBridgeImpl.h',
                'src/WebGeolocationServiceMock.cpp',
                'src/WebGlyphCache.cpp',
                'src/WebGraphicsContext3D.cpp',
                'src/WebGraphicsContext3DDefaultImpl.cpp',
                'src/WebGraphicsContext3DDefaultImpl.h',
                'src/WebHistoryItem.cpp',
                'src/WebHTTPBody.cpp',
                'src/WebIDBCallbacksImpl.cpp',
                'src/WebIDBCallbacksImpl.h',
                'src/WebIDBCursorImpl.cpp',
                'src/WebIDBCursorImpl.h',
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
                'src/WebKit.cpp',
                'src/WebLabelElement.cpp',
                'src/WebMediaElement.cpp',
                'src/WebMediaPlayerClientImpl.cpp',
                'src/WebMediaPlayerClientImpl.h',
                'src/WebNamedNodeMap.cpp',
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
                'src/WebSpeechInputControllerMockImpl.cpp',
                'src/WebSpeechInputControllerMockImpl.h',
                'src/WebStorageAreaImpl.cpp',
                'src/WebStorageAreaImpl.h',
                'src/WebStorageEventDispatcherImpl.cpp',
                'src/WebStorageEventDispatcherImpl.h',
                'src/WebStorageNamespaceImpl.cpp',
                'src/WebStorageNamespaceImpl.h',
                'src/WebString.cpp',
                'src/WebTextRun.cpp',
                'src/WebURL.cpp',
                'src/WebURLLoadTiming.cpp',
                'src/WebURLRequest.cpp',
                'src/WebURLRequestPrivate.h',
                'src/WebURLResponse.cpp',
                'src/WebURLResponsePrivate.h',
                'src/WebURLError.cpp',
                'src/WebViewImpl.cpp',
                'src/WebViewImpl.h',
                'src/WebWorkerBase.cpp',
                'src/WebWorkerBase.h',
                'src/WebWorkerClientImpl.cpp',
                'src/WebWorkerClientImpl.h',
                'src/WebWorkerImpl.cpp',
                'src/WebWorkerImpl.h',
                'src/WrappedResourceRequest.h',
                'src/WrappedResourceResponse.h',
                'src/win/WebInputEventFactory.cpp',
                'src/win/WebScreenInfoFactory.cpp',
            ],
            'conditions': [
                ['inside_chromium_build==1 and OS=="win"', {
                    'type': '<(component)',

                    'conditions': [
                        ['component=="shared_library"', {
                            'defines': [
                                'WEBKIT_DLL',
                            ],
                            'dependencies': [
                                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore_bindings',
                                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                                '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
                                '<(chromium_src_dir)/third_party/icu/icu.gyp:*',
                                '<(chromium_src_dir)/third_party/libjpeg/libjpeg.gyp:libjpeg',
                                '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
                                '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
                                '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
                                '<(chromium_src_dir)/third_party/modp_b64/modp_b64.gyp:modp_b64',
                                '<(chromium_src_dir)/third_party/nss/nss.gyp:*',
                                '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
                                '<(chromium_src_dir)/third_party/zlib/zlib.gyp:zlib',
                                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
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
                        }],
                    ],
                }, {
                    'type': '<(webkit_target_type)'
                }],
                ['OS=="linux" or OS=="freebsd"', {
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
                        '<(chromium_src_dir)/build/linux/system.gyp:gtk',
                        '<(chromium_src_dir)/build/linux/system.gyp:x11',
                    ],
                    'include_dirs': [
                        'public/x11',
                        'public/gtk',
                        'public/linux',
                    ],
                }, { # else: OS!="linux" and OS!="freebsd"
                    'sources/': [
                        ['exclude', '/gtk/'],
                        ['exclude', '/x11/'],
                        ['exclude', '/linux/'],
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        'public/mac',
                    ],
                    'sources/': [
                        ['exclude', 'Skia\\.cpp$'],
                    ],
                    'variables': {
                        # FIXME: Turn on warnings on other platforms and for
                        # other targets.
                        'chromium_code': 1,
                    }
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
                }],
                ['"ENABLE_3D_CANVAS=1" in feature_defines', {
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
                    'dependencies': ['concatenated_devtools_js'],
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
                            'files/': [['exclude', '\\.js$']],
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
                'inputs': [
                    '<(chromium_src_dir)/webkit/build/generate_devtools_html.py',
                    # See issue 29695: WebKit.gypi is a source file for devtools.html.
                    'WebKit.gypi',
                    '../../WebCore/inspector/front-end/inspector.html',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/devtools.html'],
                'action': ['python', '<@(_inputs)', '<@(_outputs)', '<@(devtools_files)'],
            }],
        },
        {
            'target_name': 'concatenated_devtools_js',
            'type': 'none',
            'dependencies': [
                'devtools_html',
                '../../WebCore/WebCore.gyp/WebCore.gyp:inspector_protocol_sources'
            ],
            'sources': ['<(PRODUCT_DIR)/resources/inspector/DevTools.js'],
            'actions': [{
                'action_name': 'concatenate_devtools_js',
                'script_name': '<(chromium_src_dir)/webkit/build/concatenate_js_files.py',
                'input_page': '<(PRODUCT_DIR)/resources/inspector/devtools.html',
                'inputs': [
                    '<@(_script_name)',
                    '<@(_input_page)',
                    '<@(webinspector_files)',
                    '<@(devtools_files)',
                    '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendStub.js',
                ],
                'search_path': [
                    '../../WebCore/inspector/front-end',
                    'src/js',
                    '<(SHARED_INTERMEDIATE_DIR)/webcore',
                    '<(chromium_src_dir)/v8/tools',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/DevTools.js'],
                'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_search_path)', '<@(_outputs)'],
            }],
        },

        {
            'target_name': 'webkit_unit_tests',
            'conditions': [
                # FIXME: make webkit unit tests working for multi dll build.
                ['inside_chromium_build==1 and OS=="win" and component=="shared_library"', {
                    'type': 'none',
                }, {
                    'type': 'executable',
                    'msvs_guid': '7CEFE800-8403-418A-AD6A-2D52C6FC3EAD',
                    'dependencies': [
                        'webkit',
                        '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                        '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                        '<(chromium_src_dir)/base/base.gyp:base',
                        '<(chromium_src_dir)/base/base.gyp:base_i18n',
                        '<(chromium_src_dir)/base/base.gyp:test_support_base',
                        '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:blob',
                        '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                    ],
                    'include_dirs': [
                        'public',
                        'src',
                    ],
                    'sources': [
                        'tests/DragImageTest.cpp',
                        'tests/IDBBindingUtilitiesTest.cpp',
                        'tests/IDBKeyPathTest.cpp',
                        'tests/KeyboardTest.cpp',
                        'tests/KURLTest.cpp',
                        'tests/PODArenaTest.cpp',
                        'tests/PODIntervalTreeTest.cpp',
                        'tests/PODRedBlackTreeTest.cpp',
                        'tests/RunAllTests.cpp',
                        'tests/TilingDataTest.cpp',
                        'tests/TreeTestHelpers.cpp',
                        'tests/TreeTestHelpers.h',
                    ],
                    'conditions': [
                        ['OS=="win"', {
                            'sources': [
                                # FIXME: Port PopupMenuTest and WebFrameTest to Linux and Mac.
                                'tests/PopupMenuTest.cpp',
                                'tests/TransparencyWinTest.cpp',
                                'tests/UniscribeHelperTest.cpp',
                                'tests/WebFrameTest.cpp',
                            ],
                        }],
                        ['OS=="mac"', {
                            'sources!': [
                                # FIXME: Port DragImageTest to Mac.
                                'tests/DragImageTest.cpp',
                            ],
                        }],
                        ['OS=="linux"', {
                            'sources': [
                                'tests/WebInputEventFactoryTestGtk.cpp',
                            ],
                            'include_dirs': [
                                'public/gtk',
                            ],
                        }],
                    ],
                }],
            ],
        },
        {
            'target_name': 'ImageDiff',
            'type': 'executable',
            'dependencies': [
                'webkit',
                '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
                '<(chromium_src_dir)/gfx/gfx.gyp:gfx',
            ],
            'include_dirs': [
                '../../JavaScriptCore',
                '<(DEPTH)',
            ],
            'sources': [
                '../../WebKitTools/DumpRenderTree/chromium/ImageDiff.cpp',
            ],
        },
        {
            'target_name': 'DumpRenderTree',
            'type': 'executable',
            'mac_bundle': 1,
            'dependencies': [
                'ImageDiff',
                'inspector_resources',
                'webkit',
                '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf_config',
                '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:blob',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:copy_npapi_layout_test_plugin',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib'
            ],
            'include_dirs': [
                '.',
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
                    'dependencies': ['LayoutTestHelper'],

                    'resource_include_dirs': ['<(SHARED_INTERMEDIATE_DIR)/webkit'],
                    'sources': [
                        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
                    ],
                    'conditions': [
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
                            ],
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
                                'repack_path': '<(chromium_src_dir)/tools/data_pack/repack.py',
                                'pak_inputs': [
                                    '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
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
                    'dependencies': ['LayoutTestHelper'],

                    'mac_bundle_resources': [
                        '<(ahem_path)',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher100.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher200.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher300.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher400.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher500.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher600.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher700.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher800.ttf',
                        '../../WebKitTools/DumpRenderTree/fonts/WebKitWeightWatcher900.ttf',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
                    ],
                    # Workaround for http://code.google.com/p/gyp/issues/detail?id=160
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/DumpRenderTree.app/Contents/PlugIns/',
                        'files': ['<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/'],
                    }],
                },{ # OS!="mac"
                    'sources/': [
                        # .mm is already excluded by common.gypi
                        ['exclude', 'Mac\\.cpp$'],
                    ]
                }],
                ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
                        '<(chromium_src_dir)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        'public/gtk',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '../../WebKitTools/DumpRenderTree/chromium/fonts.conf',
                            '<(INTERMEDIATE_DIR)/repack/DumpRenderTree.pak',
                        ]
                    }],
                },{ # OS!="linux" and OS!="freebsd" and OS!="openbsd" and OS!="solaris"
                    'sources/': [
                        ['exclude', '(Gtk|Linux)\\.cpp$']
                    ]
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="win"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['../../WebKitTools/DumpRenderTree/chromium/LayoutTestHelperWin.cpp'],
            }],
        }],
        ['OS=="mac"', {
            'targets': [
                {
                    'target_name': 'LayoutTestHelper',
                    'type': 'executable',
                    'sources': ['../../WebKitTools/DumpRenderTree/chromium/LayoutTestHelper.mm'],
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                        ],
                    },
                },
            ],
        }],
    ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

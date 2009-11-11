#
# Copyright (C) 2009 Google Inc. All rights reserved.
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
        'features.gypi',
    ],
    'variables': {
        'conditions': [
            # Location of the chromium src directory.
            ['inside_chromium_build==0', {
                # Webkit is being built outside of the full chromium project.
                # e.g. via build-webkit --chromium
                'chromium_src_dir': '.',
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '../../../..',
            }],
            # We can't turn on warnings on Windows and Linux until we upstream the
            # WebKit API.
            ['OS=="mac"', {
                'chromium_code': 1,
            }],
        ],
    },
    'targets': [
        {
            'target_name': 'webkit',
            'type': '<(library)',
            'msvs_guid': '5ECEC9E5-8F23-47B6-93E0-C3B328B3BE65',
            'dependencies': [
                '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
            ],
            'include_dirs': [
                'public',
                'src',
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION',
            ],
            'sources': [
                'public/gtk/WebInputEventFactory.h',
                'public/linux/WebFontRendering.h',
                'public/x11/WebScreenInfoFactory.h',
                'public/mac/WebInputEventFactory.h',
                'public/mac/WebScreenInfoFactory.h',
                'public/WebAccessibilityCache.h',
                'public/WebAccessibilityObject.h',
                'public/WebAccessibilityRole.h',
                'public/WebApplicationCacheHost.h',
                'public/WebApplicationCacheHostClient.h',
                'public/WebBindings.h',
                'public/WebCache.h',
                'public/WebCanvas.h',
                'public/WebClipboard.h',
                'public/WebColor.h',
                'public/WebColorName.h',
                'public/WebCommon.h',
                'public/WebCommonWorkerClient.h',
                'public/WebCompositionCommand.h',
                'public/WebConsoleMessage.h',
                'public/WebContextMenuData.h',
                'public/WebCookie.h',
                'public/WebCrossOriginPreflightResultCache.h',
                'public/WebCString.h',
                'public/WebCursorInfo.h',
                'public/WebData.h',
                'public/WebDatabase.h',
                'public/WebDatabaseObserver.h',
                'public/WebDataSource.h',
                'public/WebDevToolsAgent.h',
                'public/WebDevToolsAgentClient.h',
                'public/WebDevToolsFrontend.h',
                'public/WebDevToolsFrontendClient.h',
                'public/WebDragData.h',
                'public/WebEditingAction.h',
                'public/WebElement.h',
                'public/WebFileChooserCompletion.h',
                'public/WebFindOptions.h',
                'public/WebFrame.h',
                'public/WebFrameClient.h',
                'public/WebFontCache.h',
                'public/WebFormElement.h',
                'public/WebHistoryItem.h',
                'public/WebHTTPBody.h',
                'public/WebImage.h',
                'public/WebInputElement.h',
                'public/WebInputEvent.h',
                'public/WebKit.h',
                'public/WebKitClient.h',
                'public/WebLocalizedString.h',
                'public/WebMediaPlayer.h',
                'public/WebMediaPlayerAction.h',
                'public/WebMediaPlayerClient.h',
                'public/WebMessagePortChannel.h',
                'public/WebMessagePortChannelClient.h',
                'public/WebMimeRegistry.h',
                'public/WebNavigationType.h',
                'public/WebNode.h',
                'public/WebNonCopyable.h',
                'public/WebNotification.h',
                'public/WebNotificationPresenter.h',
                'public/WebNotificationPermissionCallback.h',
                'public/WebPasswordAutocompleteListener.h',
                'public/WebPasswordFormData.h',
                'public/WebPlugin.h',
                'public/WebPluginContainer.h',
                'public/WebPluginListBuilder.h',
                'public/WebPoint.h',
                'public/WebPopupMenu.h',
                'public/WebPopupMenuInfo.h',
                'public/WebRange.h',
                'public/WebRect.h',
                'public/WebRuntimeFeatures.h',
                'public/WebScreenInfo.h',
                'public/WebScriptController.h',
                'public/WebScriptSource.h',
                'public/WebSearchableFormData.h',
                'public/WebSecurityOrigin.h',
                'public/WebSecurityPolicy.h',
                'public/WebSettings.h',
                'public/WebSharedWorker.h',
                'public/WebSharedWorkerRepository.h',
                'public/WebSize.h',
                'public/WebSocketStreamError.h',
                'public/WebSocketStreamHandle.h',
                'public/WebSocketStreamHandleClient.h',
                'public/WebStorageArea.h',
                'public/WebStorageEventDispatcher.h',
                'public/WebStorageNamespace.h',
                'public/WebString.h',
                'public/WebTextAffinity.h',
                'public/WebTextDirection.h',
                'public/WebURL.h',
                'public/WebURLError.h',
                'public/WebURLLoader.h',
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
                'src/AssertMatchingEnums.cpp',
                'src/AutocompletePopupMenuClient.cpp',
                'src/AutocompletePopupMenuClient.h',
                'src/BackForwardListClientImpl.cpp',
                'src/BackForwardListClientImpl.h',
                'src/ChromeClientImpl.cpp',
                'src/ChromeClientImpl.h',
                'src/ChromiumBridge.cpp',
                'src/ChromiumCurrentTime.cpp',
                'src/ChromiumThreading.cpp',
                'src/ContextMenuClientImpl.cpp',
                'src/ContextMenuClientImpl.h',
                'src/DOMUtilitiesPrivate.cpp',
                'src/DOMUtilitiesPrivate.h',
                'src/DragClientImpl.cpp',
                'src/DragClientImpl.h',
                'src/EditorClientImpl.cpp',
                'src/EditorClientImpl.h',
                'src/FrameLoaderClientImpl.cpp',
                'src/FrameLoaderClientImpl.h',
                'src/gtk/WebFontInfo.cpp',
                'src/gtk/WebFontInfo.h',
                'src/gtk/WebInputEventFactory.cpp',
                'src/InspectorClientImpl.cpp',
                'src/InspectorClientImpl.h',
                'src/linux/WebFontRendering.cpp',
                'src/x11/WebScreenInfoFactory.cpp',
                'src/mac/WebInputEventFactory.mm',
                'src/mac/WebScreenInfoFactory.mm',
                'src/LocalizedStrings.cpp',
                'src/MediaPlayerPrivateChromium.cpp',
                'src/NotificationPresenterImpl.h',
                'src/NotificationPresenterImpl.cpp',
                'src/PlatformMessagePortChannel.cpp',
                'src/PlatformMessagePortChannel.h',
                'src/ResourceHandle.cpp',
                'src/SharedWorkerRepository.cpp',
                'src/SocketStreamHandle.cpp',
                'src/StorageAreaProxy.cpp',
                'src/StorageAreaProxy.h',
                'src/StorageEventDispatcherChromium.cpp',
                'src/StorageEventDispatcherImpl.cpp',
                'src/StorageEventDispatcherImpl.h',
                'src/StorageNamespaceProxy.cpp',
                'src/StorageNamespaceProxy.h',
                'src/TemporaryGlue.h',
                'src/WebAccessibilityCache.cpp',
                'src/WebAccessibilityCacheImpl.cpp',
                'src/WebAccessibilityCacheImpl.h',
                'src/WebAccessibilityObject.cpp',
                'src/WebBindings.cpp',
                'src/WebCache.cpp',
                'src/WebColor.cpp',
                'src/WebCrossOriginPreflightResultCache.cpp',
                'src/WebCString.cpp',
                'src/WebCursorInfo.cpp',
                'src/WebData.cpp',
                'src/WebDatabase.cpp',
                'src/WebDataSourceImpl.cpp',
                'src/WebDataSourceImpl.h',
                'src/WebDragData.cpp',
                'src/WebElement.cpp',
                'src/WebFileChooserCompletionImpl.cpp',
                'src/WebFileChooserCompletionImpl.h',
                'src/WebFontCache.cpp',
                'src/WebFormElement.cpp',
                'src/WebFrameImpl.cpp',
                'src/WebFrameImpl.h',
                'src/WebHistoryItem.cpp',
                'src/WebHTTPBody.cpp',
                'src/WebImageCG.cpp',
                'src/WebImageSkia.cpp',
                'src/WebInputElement.cpp',
                'src/WebInputEvent.cpp',
                'src/WebInputEventConversion.cpp',
                'src/WebInputEventConversion.h',
                'src/WebKit.cpp',
                'src/WebMediaPlayerClientImpl.cpp',
                'src/WebMediaPlayerClientImpl.h',
                'src/WebNode.cpp',
                'src/WebNotification.cpp',
                'src/WebPasswordFormData.cpp',
                'src/WebPasswordFormUtils.cpp',
                'src/WebPasswordFormUtils.h',
                'src/WebPluginContainerImpl.h',
                'src/WebPluginContainerImpl.cpp',
                'src/WebPluginListBuilderImpl.cpp',
                'src/WebPluginListBuilderImpl.h',
                'src/WebPluginLoadObserver.cpp',
                'src/WebPluginLoadObserver.h',
                'src/WebPopupMenuImpl.cpp',
                'src/WebPopupMenuImpl.h',
                'src/WebRange.cpp',
                'src/WebRuntimeFeatures.cpp',
                'src/WebScriptController.cpp',
                'src/WebSearchableFormData.cpp',
                'src/WebSecurityOrigin.cpp',
                'src/WebSecurityPolicy.cpp',
                'src/WebSettingsImpl.cpp',
                'src/WebSettingsImpl.h',
                'src/WebSharedWorkerImpl.cpp',
                'src/WebSharedWorkerImpl.h',
                'src/WebStorageAreaImpl.cpp',
                'src/WebStorageAreaImpl.h',
                'src/WebStorageEventDispatcherImpl.cpp',
                'src/WebStorageEventDispatcherImpl.h',
                'src/WebStorageNamespaceImpl.cpp',
                'src/WebStorageNamespaceImpl.h',
                'src/WebString.cpp',
                'src/WebURL.cpp',
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
                    # Conditionally compile in GLEW and our GraphicsContext3D implementation.
                    'sources+': [
                        'src/GraphicsContext3D.cpp',
                        '<(chromium_src_dir)/third_party/glew/src/glew.c'
                    ],
                    'include_dirs+': [
                        '<(chromium_src_dir)/third_party/glew/include'
                    ],
                    'defines+': [
                        'GLEW_STATIC=1',
                        'GLEW_NO_GLU=1',
                    ],
                    'conditions': [
                        ['OS=="win"', {
                            'link_settings': {
                                'libraries': [
                                    '-lopengl32.lib',
                                ],
                            },
                        }],
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
    ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

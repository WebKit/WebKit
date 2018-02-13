cmake_minimum_required(VERSION 2.8.12)

include(WebKitCommon)
include(WebCoreMacros.cmake)

set_property(DIRECTORY . PROPERTY FOLDER "WebCore")

add_subdirectory(PAL/pal)

if (PAL_LIBRARY_TYPE MATCHES STATIC)
    add_definitions(-DSTATICALLY_LINKED_WITH_PAL=1)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(WebCore_PRIVATE_FRAMEWORK_HEADERS
    Modules/applepay/ApplePaySessionPaymentRequest.h
    Modules/applepay/Payment.h
    Modules/applepay/PaymentAuthorizationStatus.h
    Modules/applepay/PaymentContact.h
    Modules/applepay/PaymentCoordinator.h
    Modules/applepay/PaymentCoordinatorClient.h
    Modules/applepay/PaymentHeaders.h
    Modules/applepay/PaymentMerchantSession.h
    Modules/applepay/PaymentMethod.h

    Modules/applicationmanifest/ApplicationManifest.h
    Modules/applicationmanifest/ApplicationManifestParser.h

    Modules/cache/CacheQueryOptions.h
    Modules/cache/CacheStorageConnection.h
    Modules/cache/DOMCacheEngine.h

    Modules/encryptedmedia/CDMClient.h

    Modules/fetch/FetchBodyConsumer.h
    Modules/fetch/FetchBodySource.h
    Modules/fetch/FetchHeaders.h
    Modules/fetch/FetchLoader.h
    Modules/fetch/FetchLoaderClient.h

    Modules/geolocation/Coordinates.h
    Modules/geolocation/Geolocation.h
    Modules/geolocation/GeolocationClient.h
    Modules/geolocation/GeolocationController.h
    Modules/geolocation/GeolocationError.h
    Modules/geolocation/GeolocationPosition.h
    Modules/geolocation/Geoposition.h
    Modules/geolocation/PositionCallback.h
    Modules/geolocation/PositionError.h
    Modules/geolocation/PositionErrorCallback.h
    Modules/geolocation/PositionOptions.h

    Modules/indexeddb/IDBActiveDOMObject.h
    Modules/indexeddb/IDBDatabaseIdentifier.h
    Modules/indexeddb/IDBGetAllResult.h
    Modules/indexeddb/IDBGetResult.h
    Modules/indexeddb/IDBKey.h
    Modules/indexeddb/IDBKeyData.h
    Modules/indexeddb/IDBKeyPath.h
    Modules/indexeddb/IDBKeyRange.h
    Modules/indexeddb/IDBKeyRangeData.h
    Modules/indexeddb/IDBOpenDBRequest.h
    Modules/indexeddb/IDBRequest.h
    Modules/indexeddb/IDBTransaction.h
    Modules/indexeddb/IDBTransactionMode.h
    Modules/indexeddb/IDBValue.h
    Modules/indexeddb/IndexedDB.h

    Modules/indexeddb/client/IDBConnectionProxy.h
    Modules/indexeddb/client/IDBConnectionToServer.h
    Modules/indexeddb/client/IDBConnectionToServerDelegate.h
    Modules/indexeddb/client/TransactionOperation.h

    Modules/indexeddb/server/IDBBackingStore.h
    Modules/indexeddb/server/IDBConnectionToClient.h
    Modules/indexeddb/server/IDBConnectionToClientDelegate.h
    Modules/indexeddb/server/IDBServer.h
    Modules/indexeddb/server/ServerOpenDBRequest.h
    Modules/indexeddb/server/UniqueIDBDatabase.h
    Modules/indexeddb/server/UniqueIDBDatabaseConnection.h
    Modules/indexeddb/server/UniqueIDBDatabaseTransaction.h

    Modules/indexeddb/shared/IDBCursorInfo.h
    Modules/indexeddb/shared/IDBDatabaseInfo.h
    Modules/indexeddb/shared/IDBError.h
    Modules/indexeddb/shared/IDBGetAllRecordsData.h
    Modules/indexeddb/shared/IDBGetRecordData.h
    Modules/indexeddb/shared/IDBIndexInfo.h
    Modules/indexeddb/shared/IDBIterateCursorData.h
    Modules/indexeddb/shared/IDBObjectStoreInfo.h
    Modules/indexeddb/shared/IDBRequestData.h
    Modules/indexeddb/shared/IDBResourceIdentifier.h
    Modules/indexeddb/shared/IDBResultData.h
    Modules/indexeddb/shared/IDBTransactionInfo.h
    Modules/indexeddb/shared/InProcessIDBServer.h

    Modules/mediasession/MediaSessionEvents.h
    Modules/mediasession/MediaSessionMetadata.h
    Modules/mediasession/WebMediaSessionManager.h
    Modules/mediasession/WebMediaSessionManagerClient.h

    Modules/mediasource/SampleMap.h

    Modules/mediastream/DoubleRange.h
    Modules/mediastream/LongRange.h
    Modules/mediastream/MediaDevicesEnumerationRequest.h
    Modules/mediastream/MediaStreamTrack.h
    Modules/mediastream/MediaTrackConstraints.h
    Modules/mediastream/RTCController.h
    Modules/mediastream/UserMediaClient.h
    Modules/mediastream/UserMediaController.h
    Modules/mediastream/UserMediaRequest.h

    Modules/navigatorcontentutils/NavigatorContentUtilsClient.h

    Modules/notifications/Notification.h
    Modules/notifications/NotificationClient.h
    Modules/notifications/NotificationController.h
    Modules/notifications/NotificationDirection.h
    Modules/notifications/NotificationPermission.h
    Modules/notifications/NotificationPermissionCallback.h

    Modules/plugins/PluginReplacement.h
    Modules/plugins/YouTubePluginReplacement.h

    Modules/streams/ReadableStreamChunk.h
    Modules/streams/ReadableStreamSink.h
    Modules/streams/ReadableStreamSource.h

    Modules/webauthn/cbor/CBORReader.h
    Modules/webauthn/cbor/CBORValue.h
    Modules/webauthn/cbor/CBORWriter.h

    Modules/webdatabase/DatabaseDetails.h
    Modules/webdatabase/DatabaseManager.h
    Modules/webdatabase/DatabaseManagerClient.h
    Modules/webdatabase/DatabaseTracker.h
    Modules/webdatabase/OriginLock.h

    accessibility/AXObjectCache.h
    accessibility/AXTextStateChangeIntent.h
    accessibility/AccessibilityListBox.h
    accessibility/AccessibilityMenuListPopup.h
    accessibility/AccessibilityMockObject.h
    accessibility/AccessibilityNodeObject.h
    accessibility/AccessibilityObject.h
    accessibility/AccessibilityRenderObject.h

    animation/AnimationEffectTimingProperties.h
    animation/CSSPropertyBlendingClient.h
    animation/CompositeOperation.h
    animation/FillMode.h
    animation/IterationCompositeOperation.h
    animation/KeyframeAnimationOptions.h
    animation/KeyframeEffectOptions.h
    animation/PlaybackDirection.h

    bindings/IDLTypes.h

    bindings/js/BufferSource.h
    bindings/js/CachedModuleScriptLoader.h
    bindings/js/CachedModuleScriptLoaderClient.h
    bindings/js/CachedScriptFetcher.h
    bindings/js/CachedScriptSourceProvider.h
    bindings/js/CallTracer.h
    bindings/js/CallTracerTypes.h
    bindings/js/CommonVM.h
    bindings/js/DOMGCOutputConstraint.h
    bindings/js/DOMPromiseProxy.h
    bindings/js/DOMWrapperWorld.h
    bindings/js/GCController.h
    bindings/js/IDBBindingUtilities.h
    bindings/js/JSCSSRuleCustom.h
    bindings/js/JSCSSStyleDeclarationCustom.h
    bindings/js/JSCallbackData.h
    bindings/js/JSCustomElementInterface.h
    bindings/js/JSCustomXPathNSResolver.h
    bindings/js/JSDOMAbstractOperations.h
    bindings/js/JSDOMAttribute.h
    bindings/js/JSDOMBinding.h
    bindings/js/JSDOMBindingSecurity.h
    bindings/js/JSDOMBuiltinConstructor.h
    bindings/js/JSDOMBuiltinConstructorBase.h
    bindings/js/JSDOMCastedThisErrorBehavior.h
    bindings/js/JSDOMConstructor.h
    bindings/js/JSDOMConstructorBase.h
    bindings/js/JSDOMConstructorNotConstructable.h
    bindings/js/JSDOMConstructorWithDocument.h
    bindings/js/JSDOMConvert.h
    bindings/js/JSDOMConvertAny.h
    bindings/js/JSDOMConvertBase.h
    bindings/js/JSDOMConvertBoolean.h
    bindings/js/JSDOMConvertBufferSource.h
    bindings/js/JSDOMConvertCallbacks.h
    bindings/js/JSDOMConvertDate.h
    bindings/js/JSDOMConvertDictionary.h
    bindings/js/JSDOMConvertEnumeration.h
    bindings/js/JSDOMConvertEventListener.h
    bindings/js/JSDOMConvertIndexedDB.h
    bindings/js/JSDOMConvertInterface.h
    bindings/js/JSDOMConvertJSON.h
    bindings/js/JSDOMConvertNull.h
    bindings/js/JSDOMConvertNullable.h
    bindings/js/JSDOMConvertNumbers.h
    bindings/js/JSDOMConvertObject.h
    bindings/js/JSDOMConvertPromise.h
    bindings/js/JSDOMConvertRecord.h
    bindings/js/JSDOMConvertScheduledAction.h
    bindings/js/JSDOMConvertSequences.h
    bindings/js/JSDOMConvertSerializedScriptValue.h
    bindings/js/JSDOMConvertStrings.h
    bindings/js/JSDOMConvertUnion.h
    bindings/js/JSDOMConvertVariadic.h
    bindings/js/JSDOMConvertWebGL.h
    bindings/js/JSDOMConvertXPathNSResolver.h
    bindings/js/JSDOMExceptionHandling.h
    bindings/js/JSDOMGlobalObject.h
    bindings/js/JSDOMGlobalObjectTask.h
    bindings/js/JSDOMGuardedObject.h
    bindings/js/JSDOMIterator.h
    bindings/js/JSDOMMapLike.h
    bindings/js/JSDOMNamedConstructor.h
    bindings/js/JSDOMOperation.h
    bindings/js/JSDOMOperationReturningPromise.h
    bindings/js/JSDOMPromise.h
    bindings/js/JSDOMPromiseDeferred.h
    bindings/js/JSDOMWindowBase.h
    bindings/js/JSDOMWindowCustom.h
    bindings/js/JSDOMWindowProperties.h
    bindings/js/JSDOMWindowProxy.h
    bindings/js/JSDOMWrapper.h
    bindings/js/JSDOMWrapperCache.h
    bindings/js/JSDocumentCustom.h
    bindings/js/JSDynamicDowncast.h
    bindings/js/JSElementCustom.h
    bindings/js/JSErrorHandler.h
    bindings/js/JSEventCustom.h
    bindings/js/JSEventListener.h
    bindings/js/JSEventTargetCustom.h
    bindings/js/JSLazyEventListener.h
    bindings/js/JSMainThreadExecState.h
    bindings/js/JSMainThreadExecStateInstrumentation.h
    bindings/js/JSMediaListCustom.h
    bindings/js/JSNodeCustom.h
    bindings/js/JSNodeListCustom.h
    bindings/js/JSPluginElementFunctions.h
    bindings/js/JSReadableStreamPrivateConstructors.h
    bindings/js/JSStyleSheetCustom.h
    bindings/js/JSTrackCustom.h
    bindings/js/JSValueInWrappedObject.h
    bindings/js/JSWorkerGlobalScopeBase.h
    bindings/js/ModuleFetchFailureKind.h
    bindings/js/ReadableStream.h
    bindings/js/ReadableStreamDefaultController.h
    bindings/js/ScheduledAction.h
    bindings/js/ScriptCachedFrameData.h
    bindings/js/ScriptController.h
    bindings/js/ScriptModuleLoader.h
    bindings/js/ScriptSourceCode.h
    bindings/js/ScriptState.h
    bindings/js/ScriptWrappable.h
    bindings/js/ScriptWrappableInlines.h
    bindings/js/SerializedScriptValue.h
    bindings/js/StringAdaptors.h
    bindings/js/StructuredClone.h
    bindings/js/WebCoreBuiltinNames.h
    bindings/js/WebCoreJSClientData.h
    bindings/js/WebCoreTypedArrayController.h
    bindings/js/WorkerScriptController.h

    bridge/Bridge.h
    bridge/IdentifierRep.h
    bridge/NP_jsobject.h
    bridge/npruntime_impl.h
    bridge/npruntime_internal.h
    bridge/runtime_method.h
    bridge/runtime_object.h
    bridge/runtime_root.h

    bridge/c/c_instance.h
    bridge/c/c_utility.h

    bridge/jsc/BridgeJSC.h

    bridge/objc/WebScriptObject.h
    bridge/objc/WebScriptObjectPrivate.h

    contentextensions/CombinedFiltersAlphabet.h
    contentextensions/CombinedURLFilters.h
    contentextensions/CompiledContentExtension.h
    contentextensions/ContentExtension.h
    contentextensions/ContentExtensionActions.h
    contentextensions/ContentExtensionCompiler.h
    contentextensions/ContentExtensionError.h
    contentextensions/ContentExtensionParser.h
    contentextensions/ContentExtensionRule.h
    contentextensions/ContentExtensionsBackend.h
    contentextensions/ContentExtensionsDebugging.h
    contentextensions/DFA.h
    contentextensions/DFABytecode.h
    contentextensions/DFABytecodeCompiler.h
    contentextensions/DFABytecodeInterpreter.h
    contentextensions/DFACombiner.h
    contentextensions/DFANode.h
    contentextensions/ImmutableNFA.h
    contentextensions/ImmutableNFANodeBuilder.h
    contentextensions/MutableRange.h
    contentextensions/MutableRangeList.h
    contentextensions/NFA.h
    contentextensions/NFANode.h
    contentextensions/NFAToDFA.h
    contentextensions/Term.h
    contentextensions/URLFilterParser.h

    crypto/SerializedCryptoKeyWrap.h

    css/CSSCustomPropertyValue.h
    css/CSSFontFaceRule.h
    css/CSSGroupingRule.h
    css/CSSImportRule.h
    css/CSSLineBoxContainValue.h
    css/CSSMediaRule.h
    css/CSSPageRule.h
    css/CSSPrimitiveValue.h
    css/CSSProperty.h
    css/CSSReflectionDirection.h
    css/CSSRule.h
    css/CSSRuleList.h
    css/CSSStyleDeclaration.h
    css/CSSStyleRule.h
    css/CSSStyleSheet.h
    css/CSSUnknownRule.h
    css/CSSValue.h
    css/CSSValueList.h
    css/CSSVariableData.h
    css/Counter.h
    css/DeprecatedCSSOMCounter.h
    css/DeprecatedCSSOMPrimitiveValue.h
    css/DeprecatedCSSOMRGBColor.h
    css/DeprecatedCSSOMRect.h
    css/DeprecatedCSSOMValue.h
    css/DeprecatedCSSOMValueList.h
    css/LengthFunctions.h
    css/MediaList.h
    css/MediaQueryEvaluator.h
    css/MediaQueryExpression.h
    css/RGBColor.h
    css/Rect.h
    css/StyleProperties.h
    css/StyleSheet.h
    css/StyleSheetContents.h
    css/StyleSheetList.h

    css/parser/CSSParser.h
    css/parser/CSSParserMode.h
    css/parser/CSSParserToken.h
    css/parser/CSSParserTokenRange.h

    dom/ActiveDOMCallback.h
    dom/ActiveDOMObject.h
    dom/Attr.h
    dom/Attribute.h
    dom/CDATASection.h
    dom/CallbackResult.h
    dom/CharacterData.h
    dom/CollectionIndexCache.h
    dom/Comment.h
    dom/ContainerNode.h
    dom/ContextDestructionObserver.h
    dom/CustomElementReactionQueue.h
    dom/DOMException.h
    dom/DOMHighResTimeStamp.h
    dom/DOMImplementation.h
    dom/DOMRect.h
    dom/DOMRectInit.h
    dom/DOMRectList.h
    dom/DOMRectReadOnly.h
    dom/DOMTimeStamp.h
    dom/DataTransfer.h
    dom/DeviceOrientationClient.h
    dom/DeviceOrientationData.h
    dom/Document.h
    dom/DocumentEventQueue.h
    dom/DocumentFragment.h
    dom/DocumentIdentifier.h
    dom/DocumentMarker.h
    dom/DocumentMarkerController.h
    dom/DocumentOrderedMap.h
    dom/DocumentTiming.h
    dom/DocumentType.h
    dom/Element.h
    dom/ElementAncestorIterator.h
    dom/ElementChildIterator.h
    dom/ElementData.h
    dom/ElementDescendantIterator.h
    dom/ElementIterator.h
    dom/ElementIteratorAssertions.h
    dom/ElementTraversal.h
    dom/Event.h
    dom/EventInit.h
    dom/EventListener.h
    dom/EventListenerMap.h
    dom/EventModifierInit.h
    dom/EventNames.h
    dom/EventQueue.h
    dom/EventTarget.h
    dom/Exception.h
    dom/ExceptionCode.h
    dom/ExceptionData.h
    dom/ExceptionOr.h
    dom/FragmentScriptingPermission.h
    dom/GenericEventQueue.h
    dom/InlineStyleSheetOwner.h
    dom/KeyboardEvent.h
    dom/LiveNodeList.h
    dom/LoadableScript.h
    dom/MessagePort.h
    dom/MessagePortIdentifier.h
    dom/MouseEvent.h
    dom/MouseEventInit.h
    dom/MouseRelatedEvent.h
    dom/MutationEvent.h
    dom/MutationObserver.h
    dom/NameNodeList.h
    dom/NamedNodeMap.h
    dom/NativeNodeFilter.h
    dom/Node.h
    dom/NodeConstants.h
    dom/NodeFilter.h
    dom/NodeFilterCondition.h
    dom/NodeIterator.h
    dom/NodeList.h
    dom/NodeRenderStyle.h
    dom/NodeTraversal.h
    dom/OverflowEvent.h
    dom/Position.h
    dom/ProcessingInstruction.h
    dom/ProgressEvent.h
    dom/QualifiedName.h
    dom/RadioButtonGroups.h
    dom/Range.h
    dom/RangeBoundaryPoint.h
    dom/RegisteredEventListener.h
    dom/RenderedDocumentMarker.h
    dom/ScriptDisallowedScope.h
    dom/ScriptElement.h
    dom/ScriptElementCachedScriptFetcher.h
    dom/ScriptExecutionContext.h
    dom/SecurityContext.h
    dom/ShadowRoot.h
    dom/ShadowRootMode.h
    dom/SimulatedClickOptions.h
    dom/SpaceSplitString.h
    dom/StyledElement.h
    dom/SuccessOr.h
    dom/Text.h
    dom/TextEvent.h
    dom/TextEventInputType.h
    dom/Touch.h
    dom/Traversal.h
    dom/TreeScope.h
    dom/TreeWalker.h
    dom/TypedElementDescendantIterator.h
    dom/UIEvent.h
    dom/UIEventInit.h
    dom/UIEventWithKeyState.h
    dom/UserActionElementSet.h
    dom/UserGestureIndicator.h
    dom/UserTypingGestureIndicator.h
    dom/ViewportArguments.h
    dom/VisibilityChangeClient.h
    dom/VisibilityState.h
    dom/WheelEvent.h
    dom/XMLDocument.h

    dom/messageports/MessagePortChannel.h
    dom/messageports/MessagePortChannelProvider.h
    dom/messageports/MessagePortChannelRegistry.h
    dom/messageports/MessageWithMessagePorts.h

    editing/ClipboardAccessPolicy.h
    editing/CompositionUnderline.h
    editing/DictationAlternative.h
    editing/DictionaryPopupInfo.h
    editing/EditAction.h
    editing/Editing.h
    editing/EditingBehavior.h
    editing/EditingBehaviorTypes.h
    editing/EditingBoundary.h
    editing/EditingStyle.h
    editing/Editor.h
    editing/EditorDeleteAction.h
    editing/EditorInsertAction.h
    editing/FindOptions.h
    editing/FrameSelection.h
    editing/HTMLInterchange.h
    editing/SelectionRectGatherer.h
    editing/SmartReplace.h
    editing/SpellChecker.h
    editing/TextAffinity.h
    editing/TextGranularity.h
    editing/TextIterator.h
    editing/TextIteratorBehavior.h
    editing/UndoStep.h
    editing/VisiblePosition.h
    editing/VisibleSelection.h
    editing/VisibleUnits.h
    editing/WebContentReader.h
    editing/WritingDirection.h
    editing/markup.h

    fileapi/AsyncFileStream.h
    fileapi/Blob.h
    fileapi/BlobLineEndings.h
    fileapi/BlobPropertyBag.h
    fileapi/File.h
    fileapi/FileList.h

    history/BackForwardClient.h
    history/BackForwardController.h
    history/CachedFrame.h
    history/CachedFramePlatformData.h
    history/HistoryItem.h
    history/PageCache.h

    html/AttachmentTypes.h
    html/Autocapitalize.h
    html/AutocapitalizeTypes.h
    html/Autofill.h
    html/CachedHTMLCollection.h
    html/CanvasBase.h
    html/CollectionTraversal.h
    html/CollectionType.h
    html/DOMTokenList.h
    html/FormAssociatedElement.h
    html/FormNamedItem.h
    html/HTMLAnchorElement.h
    html/HTMLAppletElement.h
    html/HTMLAreaElement.h
    html/HTMLAttachmentElement.h
    html/HTMLAudioElement.h
    html/HTMLBRElement.h
    html/HTMLBaseElement.h
    html/HTMLBodyElement.h
    html/HTMLButtonElement.h
    html/HTMLCanvasElement.h
    html/HTMLCollection.h
    html/HTMLDListElement.h
    html/HTMLDirectoryElement.h
    html/HTMLDivElement.h
    html/HTMLDocument.h
    html/HTMLElement.h
    html/HTMLEmbedElement.h
    html/HTMLFieldSetElement.h
    html/HTMLFontElement.h
    html/HTMLFormControlElement.h
    html/HTMLFormControlElementWithState.h
    html/HTMLFormElement.h
    html/HTMLFrameElement.h
    html/HTMLFrameElementBase.h
    html/HTMLFrameOwnerElement.h
    html/HTMLFrameSetElement.h
    html/HTMLHRElement.h
    html/HTMLHeadElement.h
    html/HTMLHeadingElement.h
    html/HTMLHtmlElement.h
    html/HTMLIFrameElement.h
    html/HTMLImageElement.h
    html/HTMLImageLoader.h
    html/HTMLInputElement.h
    html/HTMLKeygenElement.h
    html/HTMLLIElement.h
    html/HTMLLabelElement.h
    html/HTMLLegendElement.h
    html/HTMLLinkElement.h
    html/HTMLMapElement.h
    html/HTMLMarqueeElement.h
    html/HTMLMediaElement.h
    html/HTMLMediaElementEnums.h
    html/HTMLMenuElement.h
    html/HTMLMenuItemElement.h
    html/HTMLMetaElement.h
    html/HTMLModElement.h
    html/HTMLOListElement.h
    html/HTMLObjectElement.h
    html/HTMLOptGroupElement.h
    html/HTMLOptionElement.h
    html/HTMLOptionsCollection.h
    html/HTMLParagraphElement.h
    html/HTMLParamElement.h
    html/HTMLPlugInElement.h
    html/HTMLPlugInImageElement.h
    html/HTMLPreElement.h
    html/HTMLQuoteElement.h
    html/HTMLScriptElement.h
    html/HTMLSelectElement.h
    html/HTMLStyleElement.h
    html/HTMLTableCaptionElement.h
    html/HTMLTableCellElement.h
    html/HTMLTableColElement.h
    html/HTMLTableElement.h
    html/HTMLTablePartElement.h
    html/HTMLTableRowElement.h
    html/HTMLTableSectionElement.h
    html/HTMLTextAreaElement.h
    html/HTMLTextFormControlElement.h
    html/HTMLTitleElement.h
    html/HTMLUListElement.h
    html/HTMLUnknownElement.h
    html/HTMLVideoElement.h
    html/ImageBitmap.h
    html/ImageDocument.h
    html/LabelableElement.h
    html/LinkIconCollector.h
    html/LinkIconType.h
    html/LinkRelAttribute.h
    html/MediaControllerInterface.h
    html/MediaElementSession.h
    html/MediaError.h
    html/PluginDocument.h
    html/StepRange.h
    html/TimeRanges.h
    html/TypeAhead.h
    html/URLRegistry.h
    html/URLUtils.h
    html/ValidityState.h
    html/VoidCallback.h

    html/canvas/ImageBitmapRenderingContextSettings.h
    html/canvas/WebGLAny.h
    html/canvas/WebGLContextAttributes.h

    html/forms/FileIconLoader.h

    html/parser/HTMLParserIdioms.h

    html/track/AudioTrack.h
    html/track/TextTrack.h
    html/track/TextTrackCue.h
    html/track/TextTrackList.h
    html/track/TrackBase.h
    html/track/TrackListBase.h
    html/track/VTTCue.h
    html/track/VideoTrack.h

    inspector/InspectorClient.h
    inspector/InspectorController.h
    inspector/InspectorFrontendClient.h
    inspector/InspectorFrontendClientLocal.h
    inspector/InspectorFrontendHost.h
    inspector/InspectorOverlay.h
    inspector/InspectorWebAgentBase.h
    inspector/PageScriptDebugServer.h

    inspector/agents/InspectorPageAgent.h

    loader/CookieJar.h
    loader/CrossOriginAccessControl.h
    loader/CrossOriginPreflightResultCache.h
    loader/DocumentLoader.h
    loader/DocumentWriter.h
    loader/EmptyClients.h
    loader/EmptyFrameLoaderClient.h
    loader/FetchOptions.h
    loader/FormState.h
    loader/FormSubmission.h
    loader/FrameLoadRequest.h
    loader/FrameLoader.h
    loader/FrameLoaderClient.h
    loader/FrameLoaderStateMachine.h
    loader/FrameLoaderTypes.h
    loader/FrameNetworkingContext.h
    loader/HTTPHeaderField.h
    loader/HistoryController.h
    loader/ImageLoader.h
    loader/LinkLoader.h
    loader/LinkLoaderClient.h
    loader/LoadTiming.h
    loader/LoaderStrategy.h
    loader/MediaResourceLoader.h
    loader/MixedContentChecker.h
    loader/NavigationAction.h
    loader/NetscapePlugInStreamLoader.h
    loader/PolicyChecker.h
    loader/ProgressTracker.h
    loader/ProgressTrackerClient.h
    loader/ResourceCryptographicDigest.h
    loader/ResourceLoadInfo.h
    loader/ResourceLoadNotifier.h
    loader/ResourceLoadObserver.h
    loader/ResourceLoadStatistics.h
    loader/ResourceLoader.h
    loader/ResourceLoaderOptions.h
    loader/ResourceLoaderTypes.h
    loader/ResourceTimingInformation.h
    loader/SubframeLoader.h
    loader/SubresourceLoader.h
    loader/SubstituteData.h
    loader/SubstituteResource.h
    loader/TextResourceDecoder.h
    loader/ThreadableLoader.h
    loader/ThreadableLoaderClient.h

    loader/appcache/ApplicationCache.h
    loader/appcache/ApplicationCacheHost.h
    loader/appcache/ApplicationCacheStorage.h

    loader/archive/Archive.h
    loader/archive/ArchiveResource.h

    loader/archive/mhtml/MHTMLArchive.h

    loader/cache/CachePolicy.h
    loader/cache/CachedImage.h
    loader/cache/CachedImageClient.h
    loader/cache/CachedRawResourceClient.h
    loader/cache/CachedResource.h
    loader/cache/CachedResourceClient.h
    loader/cache/CachedResourceHandle.h
    loader/cache/CachedResourceLoader.h
    loader/cache/CachedResourceRequest.h
    loader/cache/CachedStyleSheetClient.h
    loader/cache/KeepaliveRequestTracker.h
    loader/cache/MemoryCache.h

    page/ActivityState.h
    page/ActivityStateChangeObserver.h
    page/AdjustViewSizeOrNot.h
    page/AlternativeTextClient.h
    page/ApplicationStateChangeListener.h
    page/AutoplayEvent.h
    page/Base64Utilities.h
    page/CacheStorageProvider.h
    page/CaptionUserPreferences.h
    page/Chrome.h
    page/ChromeClient.h
    page/ClientOrigin.h
    page/ContextMenuClient.h
    page/ContextMenuContext.h
    page/ContextMenuController.h
    page/ContextMenuProvider.h
    page/DOMSelection.h
    page/DOMTimer.h
    page/DOMWindow.h
    page/DOMWindowExtension.h
    page/DOMWindowProperty.h
    page/DatabaseProvider.h
    page/DebugPageOverlays.h
    page/DeprecatedGlobalSettings.h
    page/DeviceClient.h
    page/DiagnosticLoggingClient.h
    page/DiagnosticLoggingKeys.h
    page/DiagnosticLoggingResultType.h
    page/DragActions.h
    page/DragClient.h
    page/DragController.h
    page/EditorClient.h
    page/EventHandler.h
    page/FocusController.h
    page/FocusDirection.h
    page/Frame.h
    page/FrameDestructionObserver.h
    page/FrameSnapshotting.h
    page/FrameTree.h
    page/FrameView.h
    page/LayerFlushThrottleState.h
    page/LayoutContext.h
    page/LayoutMilestones.h
    page/MainFrame.h
    page/MediaCanStartListener.h
    page/MediaProducer.h
    page/MemoryRelease.h
    page/Page.h
    page/PageConfiguration.h
    page/PageConsoleClient.h
    page/PageGroup.h
    page/PageOverlay.h
    page/PageOverlayController.h
    page/PerformanceLogging.h
    page/PerformanceLoggingClient.h
    page/PlugInClient.h
    page/PointerLockController.h
    page/PrintContext.h
    page/RuntimeEnabledFeatures.h
    page/ScrollToOptions.h
    page/SecurityOrigin.h
    page/SecurityOriginData.h
    page/SecurityOriginHash.h
    page/SecurityPolicy.h
    page/SettingsBase.h
    page/SocketProvider.h
    page/SuspendableTimer.h
    page/TextIndicator.h
    page/UserContentController.h
    page/UserContentProvider.h
    page/UserContentTypes.h
    page/UserContentURLPattern.h
    page/UserMessageHandler.h
    page/UserMessageHandlerDescriptor.h
    page/UserMessageHandlersNamespace.h
    page/UserScript.h
    page/UserScriptTypes.h
    page/UserStyleSheet.h
    page/UserStyleSheetTypes.h
    page/ValidationMessageClient.h
    page/ViewportConfiguration.h
    page/VisitedLinkStore.h
    page/WebCoreKeyboardUIMode.h
    page/WebKitNamespace.h
    page/WheelEventDeltaFilter.h
    page/WheelEventTestTrigger.h
    page/WindowFeatures.h

    page/animation/AnimationBase.h
    page/animation/CSSAnimationController.h

    page/csp/ContentSecurityPolicy.h
    page/csp/ContentSecurityPolicyHash.h
    page/csp/ContentSecurityPolicyResponseHeaders.h

    page/scrolling/AsyncScrollingCoordinator.h
    page/scrolling/AxisScrollSnapOffsets.h
    page/scrolling/ScrollSnapOffsetsInfo.h
    page/scrolling/ScrollingConstraints.h
    page/scrolling/ScrollingCoordinator.h
    page/scrolling/ScrollingMomentumCalculator.h
    page/scrolling/ScrollingStateFixedNode.h
    page/scrolling/ScrollingStateFrameScrollingNode.h
    page/scrolling/ScrollingStateNode.h
    page/scrolling/ScrollingStateOverflowScrollingNode.h
    page/scrolling/ScrollingStateScrollingNode.h
    page/scrolling/ScrollingStateStickyNode.h
    page/scrolling/ScrollingStateTree.h
    page/scrolling/ScrollingThread.h
    page/scrolling/ScrollingTree.h
    page/scrolling/ScrollingTreeFrameScrollingNode.h
    page/scrolling/ScrollingTreeNode.h
    page/scrolling/ScrollingTreeOverflowScrollingNode.h
    page/scrolling/ScrollingTreeScrollingNode.h
    page/scrolling/ScrollingTreeScrollingNodeDelegate.h
    page/scrolling/ThreadedScrollingTree.h

    platform/CPUMonitor.h
    platform/CalculationValue.h
    platform/ColorChooser.h
    platform/ColorChooserClient.h
    platform/ContentFilterUnblockHandler.h
    platform/ContentType.h
    platform/ContextMenu.h
    platform/ContextMenuItem.h
    platform/ControlStates.h
    platform/Cookie.h
    platform/CookiesStrategy.h
    platform/CountedUserActivity.h
    platform/Cursor.h
    platform/DateComponents.h
    platform/Decimal.h
    platform/DragData.h
    platform/DragImage.h
    platform/DragItem.h
    platform/EventTrackingRegions.h
    platform/FileChooser.h
    platform/FileHandle.h
    platform/FileMetadata.h
    platform/FileMonitor.h
    platform/FileStream.h
    platform/FileStreamClient.h
    platform/FileSystem.h
    platform/FloatConversion.h
    platform/GenericTaskQueue.h
    platform/HostWindow.h
    platform/KeyedCoding.h
    platform/KeypressCommand.h
    platform/LayoutUnit.h
    platform/Length.h
    platform/LengthBox.h
    platform/LengthPoint.h
    platform/LengthSize.h
    platform/LinkIcon.h
    platform/LocalizedStrings.h
    platform/LogInitialization.h
    platform/Logging.h
    platform/LowPowerModeNotifier.h
    platform/MIMETypeRegistry.h
    platform/MediaSample.h
    platform/MediaSelectionOption.h
    platform/NotImplemented.h
    platform/OrientationNotifier.h
    platform/PODInterval.h
    platform/PODIntervalTree.h
    platform/PODRedBlackTree.h
    platform/Pasteboard.h
    platform/PasteboardItemInfo.h
    platform/PasteboardStrategy.h
    platform/PasteboardWriterData.h
    platform/PlatformEvent.h
    platform/PlatformExportMacros.h
    platform/PlatformKeyboardEvent.h
    platform/PlatformMouseEvent.h
    platform/PlatformPasteboard.h
    platform/PlatformScreen.h
    platform/PlatformStrategies.h
    platform/PlatformTouchEvent.h
    platform/PlatformTouchPoint.h
    platform/PlatformWheelEvent.h
    platform/PopupMenu.h
    platform/PopupMenuClient.h
    platform/PopupMenuStyle.h
    platform/Process.h
    platform/PromisedBlobInfo.h
    platform/PublicSuffix.h
    platform/RectEdges.h
    platform/ReferrerPolicy.h
    platform/RemoteCommandListener.h
    platform/RuntimeApplicationChecks.h
    platform/SchemeRegistry.h
    platform/ScrollAnimator.h
    platform/ScrollTypes.h
    platform/ScrollView.h
    platform/ScrollableArea.h
    platform/Scrollbar.h
    platform/ScrollbarTheme.h
    platform/ScrollbarThemeComposite.h
    platform/SearchPopupMenu.h
    platform/SerializedPlatformRepresentation.h
    platform/SharedBuffer.h
    platform/SharedStringHash.h
    platform/SuddenTermination.h
    platform/Supplementable.h
    platform/ThemeTypes.h
    platform/ThreadCheck.h
    platform/ThreadGlobalData.h
    platform/ThreadSafeDataBuffer.h
    platform/Timer.h
    platform/URL.h
    platform/URLHash.h
    platform/URLParser.h
    platform/UserActivity.h
    platform/UserAgent.h
    platform/UserInterfaceLayoutDirection.h
    platform/ValidationBubble.h
    platform/WebGLStateTracker.h
    platform/Widget.h
    platform/WindowsKeyboardCodes.h

    platform/animation/Animation.h
    platform/animation/AnimationList.h
    platform/animation/AnimationTrigger.h
    platform/animation/AnimationUtilities.h
    platform/animation/TimingFunction.h

    platform/audio/AudioHardwareListener.h
    platform/audio/AudioSession.h
    platform/audio/AudioSourceProvider.h
    platform/audio/AudioStreamDescription.h
    platform/audio/PlatformAudioData.h
    platform/audio/PlatformMediaSession.h
    platform/audio/PlatformMediaSessionManager.h
    platform/audio/WebAudioBufferList.h

    platform/gamepad/GamepadProvider.h
    platform/gamepad/GamepadProviderClient.h
    platform/gamepad/PlatformGamepad.h

    platform/geoclue/GeolocationProviderGeoclue.h
    platform/geoclue/GeolocationProviderGeoclueClient.h

    platform/graphics/ANGLEWebKitBridge.h
    platform/graphics/AudioTrackPrivate.h
    platform/graphics/BitmapImage.h
    platform/graphics/Color.h
    platform/graphics/ColorSpace.h
    platform/graphics/ComplexTextController.h
    platform/graphics/DashArray.h
    platform/graphics/DecodingOptions.h
    platform/graphics/DisplayRefreshMonitor.h
    platform/graphics/DisplayRefreshMonitorClient.h
    platform/graphics/ExtendedColor.h
    platform/graphics/Extensions3D.h
    platform/graphics/FloatPoint.h
    platform/graphics/FloatPoint3D.h
    platform/graphics/FloatQuad.h
    platform/graphics/FloatRect.h
    platform/graphics/FloatRoundedRect.h
    platform/graphics/FloatSize.h
    platform/graphics/Font.h
    platform/graphics/FontBaseline.h
    platform/graphics/FontCache.h
    platform/graphics/FontCascade.h
    platform/graphics/FontCascadeFonts.h
    platform/graphics/FontDescription.h
    platform/graphics/FontFamilySpecificationNull.h
    platform/graphics/FontMetrics.h
    platform/graphics/FontPlatformData.h
    platform/graphics/FontRanges.h
    platform/graphics/FontSelectionAlgorithm.h
    platform/graphics/FontSelector.h
    platform/graphics/FontSelectorClient.h
    platform/graphics/FontTaggedSettings.h
    platform/graphics/GLContext.h
    platform/graphics/GeometryUtilities.h
    platform/graphics/Glyph.h
    platform/graphics/GlyphBuffer.h
    platform/graphics/GlyphMetricsMap.h
    platform/graphics/GlyphPage.h
    platform/graphics/Gradient.h
    platform/graphics/GraphicsContext.h
    platform/graphics/GraphicsContext3D.h
    platform/graphics/GraphicsContext3DAttributes.h
    platform/graphics/GraphicsContextImpl.h
    platform/graphics/GraphicsLayer.h
    platform/graphics/GraphicsLayerClient.h
    platform/graphics/GraphicsLayerFactory.h
    platform/graphics/GraphicsLayerTransform.h
    platform/graphics/GraphicsLayerUpdater.h
    platform/graphics/GraphicsTypes.h
    platform/graphics/GraphicsTypes3D.h
    platform/graphics/Icon.h
    platform/graphics/Image.h
    platform/graphics/ImageBackingStore.h
    platform/graphics/ImageBuffer.h
    platform/graphics/ImageBufferData.h
    platform/graphics/ImageFrame.h
    platform/graphics/ImageObserver.h
    platform/graphics/ImageOrientation.h
    platform/graphics/ImageSource.h
    platform/graphics/ImageTypes.h
    platform/graphics/InbandTextTrackPrivate.h
    platform/graphics/InbandTextTrackPrivateClient.h
    platform/graphics/IntPoint.h
    platform/graphics/IntPointHash.h
    platform/graphics/IntRect.h
    platform/graphics/IntRectExtent.h
    platform/graphics/IntRectHash.h
    platform/graphics/IntSize.h
    platform/graphics/IntSizeHash.h
    platform/graphics/LayoutPoint.h
    platform/graphics/LayoutRect.h
    platform/graphics/LayoutSize.h
    platform/graphics/LegacyCDMSession.h
    platform/graphics/MediaPlaybackTarget.h
    platform/graphics/MediaPlaybackTargetClient.h
    platform/graphics/MediaPlaybackTargetContext.h
    platform/graphics/MediaPlaybackTargetPicker.h
    platform/graphics/MediaPlayer.h
    platform/graphics/MediaPlayerEnums.h
    platform/graphics/NativeImage.h
    platform/graphics/OpenGLESShims.h
    platform/graphics/OpenGLShims.h
    platform/graphics/Path.h
    platform/graphics/PathUtilities.h
    platform/graphics/Pattern.h
    platform/graphics/PlatformDisplay.h
    platform/graphics/PlatformLayer.h
    platform/graphics/PlatformMediaResourceLoader.h
    platform/graphics/PlatformTextTrack.h
    platform/graphics/PlatformTimeRanges.h
    platform/graphics/Region.h
    platform/graphics/RoundedRect.h
    platform/graphics/ShadowBlur.h
    platform/graphics/StringTruncator.h
    platform/graphics/TextRun.h
    platform/graphics/TiledBacking.h
    platform/graphics/TrackPrivateBase.h
    platform/graphics/VideoTrackPrivate.h
    platform/graphics/WidthCache.h
    platform/graphics/WindRule.h

    platform/graphics/filters/FilterOperation.h
    platform/graphics/filters/FilterOperations.h

    platform/graphics/gpu/GPUBuffer.h
    platform/graphics/gpu/GPUCommandQueue.h
    platform/graphics/gpu/GPUDevice.h
    platform/graphics/gpu/GPUFunction.h
    platform/graphics/gpu/GPULibrary.h

    platform/graphics/opengl/Extensions3DOpenGL.h
    platform/graphics/opengl/Extensions3DOpenGLCommon.h
    platform/graphics/opengl/Extensions3DOpenGLES.h
    platform/graphics/opengl/TemporaryOpenGLSetting.h

    platform/graphics/opentype/OpenTypeMathData.h
    platform/graphics/opentype/OpenTypeVerticalData.h

    platform/graphics/transforms/AffineTransform.h
    platform/graphics/transforms/TransformOperation.h
    platform/graphics/transforms/TransformOperations.h
    platform/graphics/transforms/TransformationMatrix.h

    platform/mediastream/CaptureDevice.h
    platform/mediastream/CaptureDeviceManager.h
    platform/mediastream/MediaConstraints.h
    platform/mediastream/MediaStreamPrivate.h
    platform/mediastream/MediaStreamRequest.h
    platform/mediastream/MediaStreamTrackPrivate.h
    platform/mediastream/RealtimeMediaSource.h
    platform/mediastream/RealtimeMediaSourceCapabilities.h
    platform/mediastream/RealtimeMediaSourceCenter.h
    platform/mediastream/RealtimeMediaSourceSettings.h
    platform/mediastream/RealtimeMediaSourceSupportedConstraints.h
    platform/mediastream/WebAudioSourceProvider.h

    platform/mediastream/libwebrtc/LibWebRTCMacros.h
    platform/mediastream/libwebrtc/LibWebRTCProvider.h
    platform/mediastream/libwebrtc/LibWebRTCProviderCocoa.h

    platform/mock/DeviceOrientationClientMock.h
    platform/mock/MediaPlaybackTargetMock.h
    platform/mock/MediaPlaybackTargetPickerMock.h
    platform/mock/MockRealtimeAudioSource.h
    platform/mock/MockRealtimeMediaSource.h
    platform/mock/MockRealtimeMediaSourceCenter.h
    platform/mock/MockRealtimeVideoSource.h

    platform/network/AuthenticationChallengeBase.h
    platform/network/AuthenticationClient.h
    platform/network/BlobData.h
    platform/network/BlobDataFileReference.h
    platform/network/BlobPart.h
    platform/network/BlobRegistry.h
    platform/network/BlobRegistryImpl.h
    platform/network/CacheValidation.h
    platform/network/CookieStorage.h
    platform/network/Credential.h
    platform/network/CredentialBase.h
    platform/network/CredentialStorage.h
    platform/network/DNS.h
    platform/network/FormData.h
    platform/network/HTTPHeaderMap.h
    platform/network/HTTPParsers.h
    platform/network/NetworkLoadMetrics.h
    platform/network/NetworkStorageSession.h
    platform/network/NetworkingContext.h
    platform/network/ParsedContentRange.h
    platform/network/PingHandle.h
    platform/network/PlatformCookieJar.h
    platform/network/ProtectionSpace.h
    platform/network/ProtectionSpaceBase.h
    platform/network/ProtectionSpaceHash.h
    platform/network/ProxyServer.h
    platform/network/ResourceErrorBase.h
    platform/network/ResourceHandle.h
    platform/network/ResourceHandleClient.h
    platform/network/ResourceLoadPriority.h
    platform/network/ResourceRequestBase.h
    platform/network/ResourceResponseBase.h
    platform/network/SocketStreamError.h
    platform/network/SocketStreamHandle.h
    platform/network/SocketStreamHandleClient.h
    platform/network/StoredCredentialsPolicy.h
    platform/network/SynchronousLoaderClient.h

    platform/sql/SQLValue.h
    platform/sql/SQLiteDatabase.h
    platform/sql/SQLiteDatabaseTracker.h
    platform/sql/SQLiteDatabaseTrackerClient.h
    platform/sql/SQLiteFileSystem.h
    platform/sql/SQLiteStatement.h
    platform/sql/SQLiteTransaction.h

    platform/text/BidiContext.h
    platform/text/StringWithDirection.h
    platform/text/TextBoundaries.h
    platform/text/TextCheckerClient.h
    platform/text/TextChecking.h
    platform/text/TextCodec.h
    platform/text/TextEncoding.h
    platform/text/TextEncodingRegistry.h
    platform/text/TextFlags.h
    platform/text/UnicodeBidi.h
    platform/text/WritingMode.h

    plugins/PluginData.h
    plugins/PluginInfoProvider.h
    plugins/PluginViewBase.h
    plugins/npapi.h
    plugins/npfunctions.h
    plugins/npruntime.h
    plugins/nptypes.h

    rendering/BreakLines.h
    rendering/ClipPathOperation.h
    rendering/ClipRect.h
    rendering/FloatingObjects.h
    rendering/GapRects.h
    rendering/HitTestLocation.h
    rendering/HitTestRequest.h
    rendering/HitTestResult.h
    rendering/InlineBox.h
    rendering/InlineFlowBox.h
    rendering/LayerFragment.h
    rendering/MarkerSubrange.h
    rendering/OrderIterator.h
    rendering/OverlapTestRequestClient.h
    rendering/Pagination.h
    rendering/PaintInfo.h
    rendering/PaintPhase.h
    rendering/RenderAttachment.h
    rendering/RenderBlock.h
    rendering/RenderBlockFlow.h
    rendering/RenderBox.h
    rendering/RenderBoxModelObject.h
    rendering/RenderElement.h
    rendering/RenderEmbeddedObject.h
    rendering/RenderFlexibleBox.h
    rendering/RenderFrame.h
    rendering/RenderFrameBase.h
    rendering/RenderImage.h
    rendering/RenderImageResource.h
    rendering/RenderInline.h
    rendering/RenderLayer.h
    rendering/RenderLayerBacking.h
    rendering/RenderLayerCompositor.h
    rendering/RenderLayerModelObject.h
    rendering/RenderLineBoxList.h
    rendering/RenderMedia.h
    rendering/RenderObject.h
    rendering/RenderObjectEnums.h
    rendering/RenderOverflow.h
    rendering/RenderPtr.h
    rendering/RenderReplaced.h
    rendering/RenderSelectionInfo.h
    rendering/RenderSnapshottedPlugIn.h
    rendering/RenderText.h
    rendering/RenderTextControl.h
    rendering/RenderTextLineBoxes.h
    rendering/RenderTheme.h
    rendering/RenderThemeCocoa.h
    rendering/RenderThemeIOS.h
    rendering/RenderThemeWin.h
    rendering/RenderTreeAsText.h
    rendering/RenderVideo.h
    rendering/RenderView.h
    rendering/RenderWidget.h
    rendering/RootInlineBox.h
    rendering/ScrollAlignment.h
    rendering/SelectionRangeData.h
    rendering/SimpleLineLayout.h
    rendering/SimpleLineLayoutCoverage.h

    rendering/line/LineWidth.h
    rendering/line/TrailingObjects.h

    rendering/shapes/Shape.h
    rendering/shapes/ShapeOutsideInfo.h

    rendering/style/BasicShapes.h
    rendering/style/BorderData.h
    rendering/style/BorderValue.h
    rendering/style/CounterDirectives.h
    rendering/style/DataRef.h
    rendering/style/FillLayer.h
    rendering/style/GapLength.h
    rendering/style/GridArea.h
    rendering/style/GridLength.h
    rendering/style/GridPosition.h
    rendering/style/GridTrackSize.h
    rendering/style/LineClampValue.h
    rendering/style/NinePieceImage.h
    rendering/style/OutlineValue.h
    rendering/style/RenderStyle.h
    rendering/style/RenderStyleConstants.h
    rendering/style/SVGRenderStyle.h
    rendering/style/SVGRenderStyleDefs.h
    rendering/style/ShadowData.h
    rendering/style/ShapeValue.h
    rendering/style/StyleBackgroundData.h
    rendering/style/StyleBoxData.h
    rendering/style/StyleContentAlignmentData.h
    rendering/style/StyleCustomPropertyData.h
    rendering/style/StyleDashboardRegion.h
    rendering/style/StyleDeprecatedFlexibleBoxData.h
    rendering/style/StyleFilterData.h
    rendering/style/StyleFlexibleBoxData.h
    rendering/style/StyleGridData.h
    rendering/style/StyleGridItemData.h
    rendering/style/StyleImage.h
    rendering/style/StyleInheritedData.h
    rendering/style/StyleMarqueeData.h
    rendering/style/StyleMultiColData.h
    rendering/style/StyleRareInheritedData.h
    rendering/style/StyleRareNonInheritedData.h
    rendering/style/StyleReflection.h
    rendering/style/StyleSelfAlignmentData.h
    rendering/style/StyleSurroundData.h
    rendering/style/StyleTransformData.h
    rendering/style/StyleVisualData.h
    rendering/style/TextSizeAdjustment.h
    rendering/style/WillChangeData.h

    replay/UserInputBridge.h

    storage/Storage.h
    storage/StorageArea.h
    storage/StorageEventDispatcher.h
    storage/StorageMap.h
    storage/StorageNamespace.h
    storage/StorageNamespaceProvider.h
    storage/StorageType.h

    style/StyleChange.h
    style/StyleScope.h
    style/StyleValidity.h

    svg/SVGLengthContext.h
    svg/SVGLengthValue.h
    svg/SVGParsingError.h
    svg/SVGStringListValues.h
    svg/SVGTests.h
    svg/SVGUnitTypes.h

    svg/graphics/SVGImageCache.h

    svg/properties/SVGAnimatedProperty.h
    svg/properties/SVGAnimatedPropertyDescription.h
    svg/properties/SVGAnimatedPropertyMacros.h
    svg/properties/SVGAttributeToPropertyMap.h
    svg/properties/SVGPropertyInfo.h
    svg/properties/SVGPropertyTraits.h

    testing/MockGamepad.h
    testing/MockGamepadProvider.h

    workers/WorkerDebuggerProxy.h
    workers/WorkerLoaderProxy.h
    workers/WorkerRunLoop.h
    workers/WorkerScriptLoader.h
    workers/WorkerScriptLoaderClient.h
    workers/WorkerThread.h
    workers/WorkerType.h

    workers/service/SWClientConnection.h
    workers/service/ServiceWorkerClientData.h
    workers/service/ServiceWorkerClientIdentifier.h
    workers/service/ServiceWorkerClientQueryOptions.h
    workers/service/ServiceWorkerClientType.h
    workers/service/ServiceWorkerContextData.h
    workers/service/ServiceWorkerData.h
    workers/service/ServiceWorkerFetchResult.h
    workers/service/ServiceWorkerIdentifier.h
    workers/service/ServiceWorkerJob.h
    workers/service/ServiceWorkerJobClient.h
    workers/service/ServiceWorkerJobData.h
    workers/service/ServiceWorkerJobDataIdentifier.h
    workers/service/ServiceWorkerJobType.h
    workers/service/ServiceWorkerProvider.h
    workers/service/ServiceWorkerRegistrationData.h
    workers/service/ServiceWorkerRegistrationKey.h
    workers/service/ServiceWorkerRegistrationOptions.h
    workers/service/ServiceWorkerTypes.h
    workers/service/ServiceWorkerUpdateViaCache.h

    workers/service/context/SWContextManager.h
    workers/service/context/ServiceWorkerDebuggable.h
    workers/service/context/ServiceWorkerFetch.h
    workers/service/context/ServiceWorkerInspectorProxy.h
    workers/service/context/ServiceWorkerThread.h
    workers/service/context/ServiceWorkerThreadProxy.h

    workers/service/server/RegistrationDatabase.h
    workers/service/server/RegistrationStore.h
    workers/service/server/SWOriginStore.h
    workers/service/server/SWServer.h
    workers/service/server/SWServerRegistration.h
    workers/service/server/SWServerToContextConnection.h
    workers/service/server/SWServerWorker.h

    xml/XPathExpression.h
    xml/XPathNSResolver.h
    xml/XPathNodeSet.h
    xml/XPathResult.h
    xml/XPathValue.h
)

set(WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/airplay"
    "${WEBCORE_DIR}/Modules/beacon"
    "${WEBCORE_DIR}/Modules/applepay"
    "${WEBCORE_DIR}/Modules/applepay/paymentrequest"
    "${WEBCORE_DIR}/Modules/cache"
    "${WEBCORE_DIR}/Modules/credentialmanagement"
    "${WEBCORE_DIR}/Modules/encryptedmedia"
    "${WEBCORE_DIR}/Modules/encryptedmedia/legacy"
    "${WEBCORE_DIR}/Modules/entriesapi"
    "${WEBCORE_DIR}/Modules/fetch"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${WEBCORE_DIR}/Modules/indexeddb"
    "${WEBCORE_DIR}/Modules/indexeddb/client"
    "${WEBCORE_DIR}/Modules/indexeddb/server"
    "${WEBCORE_DIR}/Modules/indexeddb/shared"
    "${WEBCORE_DIR}/Modules/mediacapabilities"
    "${WEBCORE_DIR}/Modules/mediacontrols/"
    "${WEBCORE_DIR}/Modules/mediasession"
    "${WEBCORE_DIR}/Modules/mediasource"
    "${WEBCORE_DIR}/Modules/mediastream"
    "${WEBCORE_DIR}/Modules/navigatorcontentutils"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/paymentrequest"
    "${WEBCORE_DIR}/Modules/plugins"
    "${WEBCORE_DIR}/Modules/quota"
    "${WEBCORE_DIR}/Modules/speech"
    "${WEBCORE_DIR}/Modules/streams"
    "${WEBCORE_DIR}/Modules/webaudio"
    "${WEBCORE_DIR}/Modules/webauthn"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/Modules/webdriver"
    "${WEBCORE_DIR}/Modules/websockets"
    "${WEBCORE_DIR}/Modules/webvr"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/animation"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/c"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/contentextensions"
    "${WEBCORE_DIR}/crypto"
    "${WEBCORE_DIR}/crypto/algorithms"
    "${WEBCORE_DIR}/crypto/keys"
    "${WEBCORE_DIR}/crypto/parameters"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/css/parser"
    "${WEBCORE_DIR}/cssjit"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/default"
    "${WEBCORE_DIR}/dom/messageports"
    "${WEBCORE_DIR}/domjit"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/canvas"
    "${WEBCORE_DIR}/html/forms"
    "${WEBCORE_DIR}/html/parser"
    "${WEBCORE_DIR}/html/shadow"
    "${WEBCORE_DIR}/html/track"
    "${WEBCORE_DIR}/inspector"
    "${WEBCORE_DIR}/inspector/agents"
    "${WEBCORE_DIR}/inspector/agents/page"
    "${WEBCORE_DIR}/inspector/agents/worker"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/loader/archive"
    "${WEBCORE_DIR}/loader/archive/mhtml"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/mathml"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/page/csp"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/crypto"
    "${WEBCORE_DIR}/platform/encryptedmedia"
    "${WEBCORE_DIR}/platform/gamepad"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm/filters"
    "${WEBCORE_DIR}/platform/graphics/displaylists"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/iso"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/mediastream"
    "${WEBCORE_DIR}/platform/mediastream/libwebrtc"
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/platform/text/icu"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/mathml"
    "${WEBCORE_DIR}/rendering/shapes"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/rendering/svg"
    "${WEBCORE_DIR}/rendering/updating"
    "${WEBCORE_DIR}/replay"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/style"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/animation"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBCORE_DIR}/svg/graphics/filters"
    "${WEBCORE_DIR}/svg/properties"
    "${WEBCORE_DIR}/websockets"
    "${WEBCORE_DIR}/workers"
    "${WEBCORE_DIR}/workers/service"
    "${WEBCORE_DIR}/workers/service/context"
    "${WEBCORE_DIR}/workers/service/server"
    "${WEBCORE_DIR}/xml"
    "${WEBCORE_DIR}/xml/parser"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
)

set(WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

set(WebCore_IDL_INCLUDES
    css
    dom
    fileapi
    html
    inspector
    page
    plugins
    storage
    svg
    workers
    xml

    Modules/airplay
    Modules/cache
    Modules/credentialmanagement
    Modules/encryptedmedia/legacy
    Modules/entriesapi
    Modules/fetch
    Modules/geolocation
    Modules/indexeddb
    Modules/mediacapabilities
    Modules/mediasession
    Modules/mediastream
    Modules/notifications
    Modules/paymentrequest
    Modules/quota
    Modules/speech
    Modules/streams
    Modules/webaudio
    Modules/webdatabase
    Modules/websockets
    Modules/webvr

    bindings/js

    html/canvas
    html/shadow
    html/track

    loader/appcache
)

set(WebCore_NON_SVG_IDL_FILES
    Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl

    Modules/beacon/NavigatorBeacon.idl

    Modules/cache/CacheQueryOptions.idl
    Modules/cache/DOMCache.idl
    Modules/cache/DOMCacheStorage.idl
    Modules/cache/DOMWindowCaches.idl
    Modules/cache/WorkerGlobalScopeCaches.idl

    Modules/credentialmanagement/BasicCredential.idl
    Modules/credentialmanagement/CredentialCreationOptions.idl
    Modules/credentialmanagement/CredentialRequestOptions.idl
    Modules/credentialmanagement/CredentialsContainer.idl
    Modules/credentialmanagement/NavigatorCredentials.idl

    Modules/entriesapi/DOMFileSystem.idl
    Modules/entriesapi/ErrorCallback.idl
    Modules/entriesapi/FileCallback.idl
    Modules/entriesapi/FileSystemDirectoryEntry.idl
    Modules/entriesapi/FileSystemDirectoryReader.idl
    Modules/entriesapi/FileSystemEntriesCallback.idl
    Modules/entriesapi/FileSystemEntry.idl
    Modules/entriesapi/FileSystemEntryCallback.idl
    Modules/entriesapi/FileSystemFileEntry.idl
    Modules/entriesapi/HTMLInputElementEntriesAPI.idl

    Modules/fetch/DOMWindowFetch.idl
    Modules/fetch/FetchBody.idl
    Modules/fetch/FetchHeaders.idl
    Modules/fetch/FetchReferrerPolicy.idl
    Modules/fetch/FetchRequest.idl
    Modules/fetch/FetchRequestCache.idl
    Modules/fetch/FetchRequestCredentials.idl
    Modules/fetch/FetchRequestInit.idl
    Modules/fetch/FetchRequestMode.idl
    Modules/fetch/FetchRequestRedirect.idl
    Modules/fetch/FetchResponse.idl
    Modules/fetch/WorkerGlobalScopeFetch.idl

    Modules/geolocation/Coordinates.idl
    Modules/geolocation/Geolocation.idl
    Modules/geolocation/Geoposition.idl
    Modules/geolocation/NavigatorGeolocation.idl
    Modules/geolocation/PositionCallback.idl
    Modules/geolocation/PositionError.idl
    Modules/geolocation/PositionErrorCallback.idl
    Modules/geolocation/PositionOptions.idl

    Modules/indexeddb/DOMWindowIndexedDatabase.idl
    Modules/indexeddb/IDBCursor.idl
    Modules/indexeddb/IDBCursorDirection.idl
    Modules/indexeddb/IDBCursorWithValue.idl
    Modules/indexeddb/IDBDatabase.idl
    Modules/indexeddb/IDBFactory.idl
    Modules/indexeddb/IDBIndex.idl
    Modules/indexeddb/IDBKeyRange.idl
    Modules/indexeddb/IDBObjectStore.idl
    Modules/indexeddb/IDBOpenDBRequest.idl
    Modules/indexeddb/IDBRequest.idl
    Modules/indexeddb/IDBTransaction.idl
    Modules/indexeddb/IDBTransactionMode.idl
    Modules/indexeddb/IDBVersionChangeEvent.idl
    Modules/indexeddb/WorkerGlobalScopeIndexedDatabase.idl

    Modules/mediacapabilities/AudioConfiguration.idl
    Modules/mediacapabilities/MediaCapabilities.idl
    Modules/mediacapabilities/MediaCapabilitiesInfo.idl
    Modules/mediacapabilities/MediaConfiguration.idl
    Modules/mediacapabilities/MediaDecodingConfiguration.idl
    Modules/mediacapabilities/MediaDecodingType.idl
    Modules/mediacapabilities/MediaEncodingConfiguration.idl
    Modules/mediacapabilities/MediaEncodingType.idl
    Modules/mediacapabilities/NavigatorMediaCapabilities.idl
    Modules/mediacapabilities/ScreenColorGamut.idl
    Modules/mediacapabilities/ScreenLuminance.idl
    Modules/mediacapabilities/VideoConfiguration.idl

    Modules/mediacontrols/MediaControlsHost.idl

    Modules/mediasession/HTMLMediaElementMediaSession.idl
    Modules/mediasession/MediaRemoteControls.idl
    Modules/mediasession/MediaSession.idl

    Modules/mediasource/DOMURLMediaSource.idl
    Modules/mediasource/MediaSource.idl
    Modules/mediasource/SourceBuffer.idl
    Modules/mediasource/SourceBufferList.idl
    Modules/mediasource/VideoPlaybackQuality.idl

    Modules/mediastream/CanvasCaptureMediaStreamTrack.idl
    Modules/mediastream/DoubleRange.idl
    Modules/mediastream/LongRange.idl
    Modules/mediastream/MediaDeviceInfo.idl
    Modules/mediastream/MediaDevices.idl
    Modules/mediastream/MediaStream.idl
    Modules/mediastream/MediaStreamEvent.idl
    Modules/mediastream/MediaStreamTrack.idl
    Modules/mediastream/MediaStreamTrackEvent.idl
    Modules/mediastream/MediaTrackConstraints.idl
    Modules/mediastream/MediaTrackSupportedConstraints.idl
    Modules/mediastream/NavigatorMediaDevices.idl
    Modules/mediastream/NavigatorUserMedia.idl
    Modules/mediastream/OverconstrainedError.idl
    Modules/mediastream/OverconstrainedErrorEvent.idl
    Modules/mediastream/RTCAnswerOptions.idl
    Modules/mediastream/RTCConfiguration.idl
    Modules/mediastream/RTCDTMFSender.idl
    Modules/mediastream/RTCDTMFToneChangeEvent.idl
    Modules/mediastream/RTCDataChannel.idl
    Modules/mediastream/RTCDataChannelEvent.idl
    Modules/mediastream/RTCIceCandidate.idl
    Modules/mediastream/RTCIceConnectionState.idl
    Modules/mediastream/RTCIceGatheringState.idl
    Modules/mediastream/RTCIceServer.idl
    Modules/mediastream/RTCIceTransport.idl
    Modules/mediastream/RTCIceTransportState.idl
    Modules/mediastream/RTCOfferAnswerOptions.idl
    Modules/mediastream/RTCOfferOptions.idl
    Modules/mediastream/RTCPeerConnection.idl
    Modules/mediastream/RTCPeerConnectionIceEvent.idl
    Modules/mediastream/RTCPeerConnectionState.idl
    Modules/mediastream/RTCRtpParameters.idl
    Modules/mediastream/RTCRtpReceiver.idl
    Modules/mediastream/RTCRtpSender.idl
    Modules/mediastream/RTCRtpTransceiver.idl
    Modules/mediastream/RTCRtpTransceiverDirection.idl
    Modules/mediastream/RTCSessionDescription.idl
    Modules/mediastream/RTCSignalingState.idl
    Modules/mediastream/RTCStatsReport.idl
    Modules/mediastream/RTCTrackEvent.idl

    Modules/navigatorcontentutils/NavigatorContentUtils.idl

    Modules/notifications/Notification.idl
    Modules/notifications/NotificationPermission.idl
    Modules/notifications/NotificationPermissionCallback.idl

    Modules/paymentrequest/MerchantValidationEvent.idl
    Modules/paymentrequest/PaymentAddress.idl
    Modules/paymentrequest/PaymentComplete.idl
    Modules/paymentrequest/PaymentCurrencyAmount.idl
    Modules/paymentrequest/PaymentDetailsBase.idl
    Modules/paymentrequest/PaymentDetailsInit.idl
    Modules/paymentrequest/PaymentDetailsModifier.idl
    Modules/paymentrequest/PaymentDetailsUpdate.idl
    Modules/paymentrequest/PaymentItem.idl
    Modules/paymentrequest/PaymentMethodData.idl
    Modules/paymentrequest/PaymentOptions.idl
    Modules/paymentrequest/PaymentRequest.idl
    Modules/paymentrequest/PaymentRequestUpdateEvent.idl
    Modules/paymentrequest/PaymentRequestUpdateEventInit.idl
    Modules/paymentrequest/PaymentResponse.idl
    Modules/paymentrequest/PaymentShippingOption.idl
    Modules/paymentrequest/PaymentShippingType.idl

    Modules/speech/DOMWindowSpeechSynthesis.idl
    Modules/speech/SpeechSynthesis.idl
    Modules/speech/SpeechSynthesisEvent.idl
    Modules/speech/SpeechSynthesisUtterance.idl
    Modules/speech/SpeechSynthesisVoice.idl

    Modules/streams/ByteLengthQueuingStrategy.idl
    Modules/streams/CountQueuingStrategy.idl
    Modules/streams/ReadableByteStreamController.idl
    Modules/streams/ReadableStream.idl
    Modules/streams/ReadableStreamBYOBReader.idl
    Modules/streams/ReadableStreamBYOBRequest.idl
    Modules/streams/ReadableStreamDefaultController.idl
    Modules/streams/ReadableStreamDefaultReader.idl
    Modules/streams/ReadableStreamSink.idl
    Modules/streams/ReadableStreamSource.idl
    Modules/streams/WritableStream.idl

    Modules/webaudio/AnalyserNode.idl
    Modules/webaudio/AudioBuffer.idl
    Modules/webaudio/AudioBufferCallback.idl
    Modules/webaudio/AudioBufferSourceNode.idl
    Modules/webaudio/AudioContext.idl
    Modules/webaudio/AudioDestinationNode.idl
    Modules/webaudio/AudioListener.idl
    Modules/webaudio/AudioNode.idl
    Modules/webaudio/AudioParam.idl
    Modules/webaudio/AudioProcessingEvent.idl
    Modules/webaudio/BiquadFilterNode.idl
    Modules/webaudio/ChannelMergerNode.idl
    Modules/webaudio/ChannelSplitterNode.idl
    Modules/webaudio/ConvolverNode.idl
    Modules/webaudio/DelayNode.idl
    Modules/webaudio/DynamicsCompressorNode.idl
    Modules/webaudio/GainNode.idl
    Modules/webaudio/MediaElementAudioSourceNode.idl
    Modules/webaudio/MediaStreamAudioDestinationNode.idl
    Modules/webaudio/MediaStreamAudioSourceNode.idl
    Modules/webaudio/OfflineAudioCompletionEvent.idl
    Modules/webaudio/OfflineAudioContext.idl
    Modules/webaudio/OscillatorNode.idl
    Modules/webaudio/PannerNode.idl
    Modules/webaudio/PeriodicWave.idl
    Modules/webaudio/ScriptProcessorNode.idl
    Modules/webaudio/WaveShaperNode.idl

    Modules/webauthn/AuthenticatorAssertionResponse.idl
    Modules/webauthn/AuthenticatorAttestationResponse.idl
    Modules/webauthn/AuthenticatorResponse.idl
    Modules/webauthn/PublicKeyCredential.idl
    Modules/webauthn/PublicKeyCredentialCreationOptions.idl
    Modules/webauthn/PublicKeyCredentialDescriptor.idl
    Modules/webauthn/PublicKeyCredentialRequestOptions.idl
    Modules/webauthn/PublicKeyCredentialType.idl

    Modules/webdatabase/DOMWindowWebDatabase.idl
    Modules/webdatabase/Database.idl
    Modules/webdatabase/DatabaseCallback.idl
    Modules/webdatabase/SQLError.idl
    Modules/webdatabase/SQLResultSet.idl
    Modules/webdatabase/SQLResultSetRowList.idl
    Modules/webdatabase/SQLStatementCallback.idl
    Modules/webdatabase/SQLStatementErrorCallback.idl
    Modules/webdatabase/SQLTransaction.idl
    Modules/webdatabase/SQLTransactionCallback.idl
    Modules/webdatabase/SQLTransactionErrorCallback.idl

    Modules/webdriver/NavigatorWebDriver.idl

    Modules/websockets/CloseEvent.idl
    Modules/websockets/WebSocket.idl

    Modules/webvr/DOMWindowWebVR.idl
    Modules/webvr/NavigatorWebVR.idl
    Modules/webvr/VRDisplay.idl
    Modules/webvr/VRDisplayCapabilities.idl
    Modules/webvr/VRDisplayEvent.idl
    Modules/webvr/VRDisplayEventReason.idl
    Modules/webvr/VREye.idl
    Modules/webvr/VREyeParameters.idl
    Modules/webvr/VRFieldOfView.idl
    Modules/webvr/VRFrameData.idl
    Modules/webvr/VRLayerInit.idl
    Modules/webvr/VRPose.idl
    Modules/webvr/VRStageParameters.idl

    animation/Animatable.idl
    animation/AnimationEffectReadOnly.idl
    animation/AnimationEffectTiming.idl
    animation/AnimationEffectTimingProperties.idl
    animation/AnimationEffectTimingReadOnly.idl
    animation/AnimationPlaybackEvent.idl
    animation/AnimationPlaybackEventInit.idl
    animation/AnimationTimeline.idl
    animation/CompositeOperation.idl
    animation/ComputedTimingProperties.idl
    animation/DocumentTimeline.idl
    animation/FillMode.idl
    animation/IterationCompositeOperation.idl
    animation/KeyframeAnimationOptions.idl
    animation/KeyframeEffect.idl
    animation/KeyframeEffectOptions.idl
    animation/KeyframeEffectReadOnly.idl
    animation/PlaybackDirection.idl
    animation/WebAnimation.idl

    crypto/CryptoAlgorithmParameters.idl
    crypto/CryptoKey.idl
    crypto/CryptoKeyPair.idl
    crypto/CryptoKeyUsage.idl
    crypto/JsonWebKey.idl
    crypto/RsaOtherPrimesInfo.idl
    crypto/SubtleCrypto.idl

    crypto/keys/CryptoAesKeyAlgorithm.idl
    crypto/keys/CryptoEcKeyAlgorithm.idl
    crypto/keys/CryptoHmacKeyAlgorithm.idl
    crypto/keys/CryptoKeyAlgorithm.idl
    crypto/keys/CryptoRsaHashedKeyAlgorithm.idl
    crypto/keys/CryptoRsaKeyAlgorithm.idl

    crypto/parameters/AesCbcCfbParams.idl
    crypto/parameters/AesCtrParams.idl
    crypto/parameters/AesGcmParams.idl
    crypto/parameters/AesKeyParams.idl
    crypto/parameters/EcKeyParams.idl
    crypto/parameters/EcdhKeyDeriveParams.idl
    crypto/parameters/EcdsaParams.idl
    crypto/parameters/HkdfParams.idl
    crypto/parameters/HmacKeyParams.idl
    crypto/parameters/Pbkdf2Params.idl
    crypto/parameters/RsaHashedImportParams.idl
    crypto/parameters/RsaHashedKeyGenParams.idl
    crypto/parameters/RsaKeyGenParams.idl
    crypto/parameters/RsaOaepParams.idl
    crypto/parameters/RsaPssParams.idl

    css/CSSFontFaceRule.idl
    css/CSSImportRule.idl
    css/CSSKeyframeRule.idl
    css/CSSKeyframesRule.idl
    css/CSSMediaRule.idl
    css/CSSNamespaceRule.idl
    css/CSSPageRule.idl
    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSStyleRule.idl
    css/CSSStyleSheet.idl
    css/CSSSupportsRule.idl
    css/CSSUnknownRule.idl
    css/DOMMatrix.idl
    css/DOMMatrix2DInit.idl
    css/DOMMatrixInit.idl
    css/DOMMatrixReadOnly.idl
    css/DeprecatedCSSOMCounter.idl
    css/DeprecatedCSSOMPrimitiveValue.idl
    css/DeprecatedCSSOMRGBColor.idl
    css/DeprecatedCSSOMRect.idl
    css/DeprecatedCSSOMValue.idl
    css/DeprecatedCSSOMValueList.idl
    css/DOMCSSNamespace.idl
    css/ElementCSSInlineStyle.idl
    css/FontFace.idl
    css/FontFaceSet.idl
    css/MediaList.idl
    css/MediaQueryList.idl
    css/MediaQueryListListener.idl
    css/StyleMedia.idl
    css/StyleSheet.idl
    css/StyleSheetList.idl
    css/WebKitCSSMatrix.idl
    css/WebKitCSSViewportRule.idl

    dom/AbortController.idl
    dom/AbortSignal.idl
    dom/AnimationEvent.idl
    dom/Attr.idl
    dom/BeforeLoadEvent.idl
    dom/BeforeUnloadEvent.idl
    dom/CDATASection.idl
    dom/CharacterData.idl
    dom/ChildNode.idl
    dom/ClipboardEvent.idl
    dom/Comment.idl
    dom/CompositionEvent.idl
    dom/CustomElementRegistry.idl
    dom/CustomEvent.idl
    dom/DOMException.idl
    dom/DOMImplementation.idl
    dom/DOMPoint.idl
    dom/DOMPointInit.idl
    dom/DOMPointReadOnly.idl
    dom/DOMQuad.idl
    dom/DOMQuadInit.idl
    dom/DOMRect.idl
    dom/DOMRectInit.idl
    dom/DOMRectList.idl
    dom/DOMRectReadOnly.idl
    dom/DOMStringList.idl
    dom/DOMStringMap.idl
    dom/DataTransfer.idl
    dom/DataTransferItem.idl
    dom/DataTransferItemList.idl
    dom/DeviceMotionEvent.idl
    dom/DeviceOrientationEvent.idl
    dom/Document.idl
    dom/DocumentAndElementEventHandlers.idl
    dom/DocumentFragment.idl
    dom/DocumentOrShadowRoot.idl
    dom/DocumentTouch.idl
    dom/DocumentType.idl
    dom/Element.idl
    dom/ErrorEvent.idl
    dom/Event.idl
    dom/EventInit.idl
    dom/EventModifierInit.idl
    dom/EventTarget.idl
    dom/FocusEvent.idl
    dom/GlobalEventHandlers.idl
    dom/HashChangeEvent.idl
    dom/InputEvent.idl
    dom/KeyboardEvent.idl
    dom/MessageChannel.idl
    dom/MessageEvent.idl
    dom/MessagePort.idl
    dom/MouseEvent.idl
    dom/MouseEventInit.idl
    dom/MutationCallback.idl
    dom/MutationEvent.idl
    dom/MutationObserver.idl
    dom/MutationRecord.idl
    dom/NamedNodeMap.idl
    dom/Node.idl
    dom/NodeFilter.idl
    dom/NodeIterator.idl
    dom/NodeList.idl
    dom/NonDocumentTypeChildNode.idl
    dom/NonElementParentNode.idl
    dom/OverflowEvent.idl
    dom/PageTransitionEvent.idl
    dom/ParentNode.idl
    dom/PopStateEvent.idl
    dom/ProcessingInstruction.idl
    dom/ProgressEvent.idl
    dom/PromiseRejectionEvent.idl
    dom/Range.idl
    dom/RequestAnimationFrameCallback.idl
    dom/SecurityPolicyViolationEvent.idl
    dom/ShadowRoot.idl
    dom/ShadowRootMode.idl
    dom/Slotable.idl
    dom/SpectreGadget.idl
    dom/StaticRange.idl
    dom/StringCallback.idl
    dom/Text.idl
    dom/TextDecoder.idl
    dom/TextEncoder.idl
    dom/TextEvent.idl
    dom/Touch.idl
    dom/TouchEvent.idl
    dom/TouchList.idl
    dom/TransitionEvent.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/UIEventInit.idl
    dom/VisibilityState.idl
    dom/WebKitAnimationEvent.idl
    dom/WebKitTransitionEvent.idl
    dom/WheelEvent.idl
    dom/XMLDocument.idl

    fileapi/Blob.idl
    fileapi/BlobCallback.idl
    fileapi/BlobLineEndings.idl
    fileapi/BlobPropertyBag.idl
    fileapi/File.idl
    fileapi/FileError.idl
    fileapi/FileList.idl
    fileapi/FileReader.idl
    fileapi/FileReaderSync.idl

    html/DOMFormData.idl
    html/DOMTokenList.idl
    html/DOMURL.idl
    html/HTMLAllCollection.idl
    html/HTMLAnchorElement.idl
    html/HTMLAppletElement.idl
    html/HTMLAreaElement.idl
    html/HTMLAttachmentElement.idl
    html/HTMLAudioElement.idl
    html/HTMLBRElement.idl
    html/HTMLBaseElement.idl
    html/HTMLBodyElement.idl
    html/HTMLButtonElement.idl
    html/HTMLCanvasElement.idl
    html/HTMLCollection.idl
    html/HTMLDListElement.idl
    html/HTMLDataElement.idl
    html/HTMLDataListElement.idl
    html/HTMLDetailsElement.idl
    html/HTMLDirectoryElement.idl
    html/HTMLDivElement.idl
    html/HTMLDocument.idl
    html/HTMLElement.idl
    html/HTMLEmbedElement.idl
    html/HTMLFieldSetElement.idl
    html/HTMLFontElement.idl
    html/HTMLFormControlsCollection.idl
    html/HTMLFormElement.idl
    html/HTMLFrameElement.idl
    html/HTMLFrameSetElement.idl
    html/HTMLHRElement.idl
    html/HTMLHeadElement.idl
    html/HTMLHeadingElement.idl
    html/HTMLHtmlElement.idl
    html/HTMLHyperlinkElementUtils.idl
    html/HTMLIFrameElement.idl
    html/HTMLImageElement.idl
    html/HTMLInputElement.idl
    html/HTMLKeygenElement.idl
    html/HTMLLIElement.idl
    html/HTMLLabelElement.idl
    html/HTMLLegendElement.idl
    html/HTMLLinkElement.idl
    html/HTMLMapElement.idl
    html/HTMLMarqueeElement.idl
    html/HTMLMediaElement.idl
    html/HTMLMenuElement.idl
    html/HTMLMenuItemElement.idl
    html/HTMLMetaElement.idl
    html/HTMLMeterElement.idl
    html/HTMLModElement.idl
    html/HTMLOListElement.idl
    html/HTMLObjectElement.idl
    html/HTMLOptGroupElement.idl
    html/HTMLOptionElement.idl
    html/HTMLOptionsCollection.idl
    html/HTMLOutputElement.idl
    html/HTMLParagraphElement.idl
    html/HTMLParamElement.idl
    html/HTMLPictureElement.idl
    html/HTMLPreElement.idl
    html/HTMLProgressElement.idl
    html/HTMLQuoteElement.idl
    html/HTMLScriptElement.idl
    html/HTMLSelectElement.idl
    html/HTMLSlotElement.idl
    html/HTMLSourceElement.idl
    html/HTMLSpanElement.idl
    html/HTMLStyleElement.idl
    html/HTMLTableCaptionElement.idl
    html/HTMLTableCellElement.idl
    html/HTMLTableColElement.idl
    html/HTMLTableElement.idl
    html/HTMLTableRowElement.idl
    html/HTMLTableSectionElement.idl
    html/HTMLTemplateElement.idl
    html/HTMLTextAreaElement.idl
    html/HTMLTimeElement.idl
    html/HTMLTitleElement.idl
    html/HTMLTrackElement.idl
    html/HTMLUListElement.idl
    html/HTMLUnknownElement.idl
    html/HTMLVideoElement.idl
    html/ImageBitmap.idl
    html/ImageBitmapOptions.idl
    html/ImageData.idl
    html/MediaController.idl
    html/MediaError.idl
    html/OffscreenCanvas.idl
    html/RadioNodeList.idl
    html/TextMetrics.idl
    html/TimeRanges.idl
    html/URLSearchParams.idl
    html/ValidityState.idl
    html/VoidCallback.idl

    html/canvas/CanvasCompositing.idl
    html/canvas/CanvasDirection.idl
    html/canvas/CanvasDrawImage.idl
    html/canvas/CanvasDrawPath.idl
    html/canvas/CanvasFillRule.idl
    html/canvas/CanvasFillStrokeStyles.idl
    html/canvas/CanvasFilters.idl
    html/canvas/CanvasGradient.idl
    html/canvas/CanvasImageData.idl
    html/canvas/CanvasImageSmoothing.idl
    html/canvas/CanvasLineCap.idl
    html/canvas/CanvasLineJoin.idl
    html/canvas/CanvasPath.idl
    html/canvas/CanvasPathDrawingStyles.idl
    html/canvas/CanvasPattern.idl
    html/canvas/CanvasRect.idl
    html/canvas/CanvasRenderingContext2D.idl
    html/canvas/CanvasShadowStyles.idl
    html/canvas/CanvasState.idl
    html/canvas/CanvasText.idl
    html/canvas/CanvasTextAlign.idl
    html/canvas/CanvasTextBaseline.idl
    html/canvas/CanvasTextDrawingStyles.idl
    html/canvas/CanvasTransform.idl
    html/canvas/CanvasUserInterface.idl
    html/canvas/ImageBitmapRenderingContext.idl
    html/canvas/ImageBitmapRenderingContextSettings.idl
    html/canvas/ImageSmoothingQuality.idl
    html/canvas/OffscreenCanvasRenderingContext2D.idl
    html/canvas/Path2D.idl
    html/canvas/WebGPUBuffer.idl
    html/canvas/WebGPUCommandBuffer.idl
    html/canvas/WebGPUCommandQueue.idl
    html/canvas/WebGPUComputeCommandEncoder.idl
    html/canvas/WebGPUComputePipelineState.idl
    html/canvas/WebGPUDepthStencilDescriptor.idl
    html/canvas/WebGPUDepthStencilState.idl
    html/canvas/WebGPUDrawable.idl
    html/canvas/WebGPUFunction.idl
    html/canvas/WebGPULibrary.idl
    html/canvas/WebGPURenderCommandEncoder.idl
    html/canvas/WebGPURenderPassAttachmentDescriptor.idl
    html/canvas/WebGPURenderPassColorAttachmentDescriptor.idl
    html/canvas/WebGPURenderPassDepthAttachmentDescriptor.idl
    html/canvas/WebGPURenderPassDescriptor.idl
    html/canvas/WebGPURenderPipelineColorAttachmentDescriptor.idl
    html/canvas/WebGPURenderPipelineDescriptor.idl
    html/canvas/WebGPURenderPipelineState.idl
    html/canvas/WebGPURenderingContext.idl
    html/canvas/WebGPUSize.idl
    html/canvas/WebGPUTexture.idl
    html/canvas/WebGPUTextureDescriptor.idl

    inspector/CommandLineAPIHost.idl
    inspector/InspectorFrontendHost.idl

    loader/appcache/DOMApplicationCache.idl

    page/BarProp.idl
    page/Crypto.idl
    page/DOMSelection.idl
    page/DOMWindow.idl
    page/EventSource.idl
    page/GlobalCrypto.idl
    page/GlobalPerformance.idl
    page/History.idl
    page/IntersectionObserver.idl
    page/IntersectionObserverCallback.idl
    page/IntersectionObserverEntry.idl
    page/Location.idl
    page/Navigator.idl
    page/NavigatorID.idl
    page/NavigatorLanguage.idl
    page/NavigatorOnLine.idl
    page/NavigatorServiceWorker.idl
    page/Performance.idl
    page/PerformanceEntry.idl
    page/PerformanceMark.idl
    page/PerformanceMeasure.idl
    page/PerformanceObserver.idl
    page/PerformanceObserverCallback.idl
    page/PerformanceObserverEntryList.idl
    page/PerformanceNavigation.idl
    page/PerformanceResourceTiming.idl
    page/PerformanceTiming.idl
    page/Screen.idl
    page/ScrollToOptions.idl
    page/VisualViewport.idl
    page/WebKitPoint.idl
    page/WindowEventHandlers.idl
    page/WindowOrWorkerGlobalScope.idl
    page/WorkerNavigator.idl

    plugins/DOMMimeType.idl
    plugins/DOMMimeTypeArray.idl
    plugins/DOMPlugin.idl
    plugins/DOMPluginArray.idl

    storage/Storage.idl
    storage/StorageEvent.idl

    workers/AbstractWorker.idl
    workers/DedicatedWorkerGlobalScope.idl
    workers/Worker.idl
    workers/WorkerGlobalScope.idl
    workers/WorkerLocation.idl
    workers/WorkerType.idl

    workers/service/ExtendableEvent.idl
    workers/service/ExtendableEventInit.idl
    workers/service/ExtendableMessageEvent.idl
    workers/service/FetchEvent.idl
    workers/service/ServiceWorker.idl
    workers/service/ServiceWorkerClient.idl
    workers/service/ServiceWorkerClientType.idl
    workers/service/ServiceWorkerClients.idl
    workers/service/ServiceWorkerContainer.idl
    workers/service/ServiceWorkerGlobalScope.idl
    workers/service/ServiceWorkerRegistration.idl
    workers/service/ServiceWorkerUpdateViaCache.idl
    workers/service/ServiceWorkerWindowClient.idl

    xml/DOMParser.idl
    xml/XMLHttpRequest.idl
    xml/XMLHttpRequestEventTarget.idl
    xml/XMLHttpRequestProgressEvent.idl
    xml/XMLHttpRequestUpload.idl
    xml/XMLSerializer.idl
    xml/XPathEvaluator.idl
    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
    xml/XSLTProcessor.idl
)

set(WebCore_SVG_IDL_FILES
    svg/SVGAElement.idl
    svg/SVGAltGlyphDefElement.idl
    svg/SVGAltGlyphElement.idl
    svg/SVGAltGlyphItemElement.idl
    svg/SVGAngle.idl
    svg/SVGAnimateColorElement.idl
    svg/SVGAnimateElement.idl
    svg/SVGAnimateMotionElement.idl
    svg/SVGAnimateTransformElement.idl
    svg/SVGAnimatedAngle.idl
    svg/SVGAnimatedBoolean.idl
    svg/SVGAnimatedEnumeration.idl
    svg/SVGAnimatedInteger.idl
    svg/SVGAnimatedLength.idl
    svg/SVGAnimatedLengthList.idl
    svg/SVGAnimatedNumber.idl
    svg/SVGAnimatedNumberList.idl
    svg/SVGAnimatedPreserveAspectRatio.idl
    svg/SVGAnimatedRect.idl
    svg/SVGAnimatedString.idl
    svg/SVGAnimatedTransformList.idl
    svg/SVGAnimationElement.idl
    svg/SVGCircleElement.idl
    svg/SVGClipPathElement.idl
    svg/SVGComponentTransferFunctionElement.idl
    svg/SVGCursorElement.idl
    svg/SVGDefsElement.idl
    svg/SVGDescElement.idl
    svg/SVGDocument.idl
    svg/SVGElement.idl
    svg/SVGEllipseElement.idl
    svg/SVGExternalResourcesRequired.idl
    svg/SVGFEBlendElement.idl
    svg/SVGFEColorMatrixElement.idl
    svg/SVGFEComponentTransferElement.idl
    svg/SVGFECompositeElement.idl
    svg/SVGFEConvolveMatrixElement.idl
    svg/SVGFEDiffuseLightingElement.idl
    svg/SVGFEDisplacementMapElement.idl
    svg/SVGFEDistantLightElement.idl
    svg/SVGFEDropShadowElement.idl
    svg/SVGFEFloodElement.idl
    svg/SVGFEFuncAElement.idl
    svg/SVGFEFuncBElement.idl
    svg/SVGFEFuncGElement.idl
    svg/SVGFEFuncRElement.idl
    svg/SVGFEGaussianBlurElement.idl
    svg/SVGFEImageElement.idl
    svg/SVGFEMergeElement.idl
    svg/SVGFEMergeNodeElement.idl
    svg/SVGFEMorphologyElement.idl
    svg/SVGFEOffsetElement.idl
    svg/SVGFEPointLightElement.idl
    svg/SVGFESpecularLightingElement.idl
    svg/SVGFESpotLightElement.idl
    svg/SVGFETileElement.idl
    svg/SVGFETurbulenceElement.idl
    svg/SVGFilterElement.idl
    svg/SVGFilterPrimitiveStandardAttributes.idl
    svg/SVGFitToViewBox.idl
    svg/SVGFontElement.idl
    svg/SVGFontFaceElement.idl
    svg/SVGFontFaceFormatElement.idl
    svg/SVGFontFaceNameElement.idl
    svg/SVGFontFaceSrcElement.idl
    svg/SVGFontFaceUriElement.idl
    svg/SVGForeignObjectElement.idl
    svg/SVGGElement.idl
    svg/SVGGlyphElement.idl
    svg/SVGGlyphRefElement.idl
    svg/SVGGradientElement.idl
    svg/SVGGraphicsElement.idl
    svg/SVGHKernElement.idl
    svg/SVGImageElement.idl
    svg/SVGLength.idl
    svg/SVGLengthList.idl
    svg/SVGLineElement.idl
    svg/SVGLinearGradientElement.idl
    svg/SVGMPathElement.idl
    svg/SVGMarkerElement.idl
    svg/SVGMaskElement.idl
    svg/SVGMatrix.idl
    svg/SVGMetadataElement.idl
    svg/SVGMissingGlyphElement.idl
    svg/SVGNumber.idl
    svg/SVGNumberList.idl
    svg/SVGPathElement.idl
    svg/SVGPathSeg.idl
    svg/SVGPathSegArcAbs.idl
    svg/SVGPathSegArcRel.idl
    svg/SVGPathSegClosePath.idl
    svg/SVGPathSegCurvetoCubicAbs.idl
    svg/SVGPathSegCurvetoCubicRel.idl
    svg/SVGPathSegCurvetoCubicSmoothAbs.idl
    svg/SVGPathSegCurvetoCubicSmoothRel.idl
    svg/SVGPathSegCurvetoQuadraticAbs.idl
    svg/SVGPathSegCurvetoQuadraticRel.idl
    svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl
    svg/SVGPathSegCurvetoQuadraticSmoothRel.idl
    svg/SVGPathSegLinetoAbs.idl
    svg/SVGPathSegLinetoHorizontalAbs.idl
    svg/SVGPathSegLinetoHorizontalRel.idl
    svg/SVGPathSegLinetoRel.idl
    svg/SVGPathSegLinetoVerticalAbs.idl
    svg/SVGPathSegLinetoVerticalRel.idl
    svg/SVGPathSegList.idl
    svg/SVGPathSegMovetoAbs.idl
    svg/SVGPathSegMovetoRel.idl
    svg/SVGPatternElement.idl
    svg/SVGPoint.idl
    svg/SVGPointList.idl
    svg/SVGPolygonElement.idl
    svg/SVGPolylineElement.idl
    svg/SVGPreserveAspectRatio.idl
    svg/SVGRadialGradientElement.idl
    svg/SVGRect.idl
    svg/SVGRectElement.idl
    svg/SVGRenderingIntent.idl
    svg/SVGSVGElement.idl
    svg/SVGScriptElement.idl
    svg/SVGSetElement.idl
    svg/SVGStopElement.idl
    svg/SVGStringList.idl
    svg/SVGStyleElement.idl
    svg/SVGSwitchElement.idl
    svg/SVGSymbolElement.idl
    svg/SVGTRefElement.idl
    svg/SVGTSpanElement.idl
    svg/SVGTests.idl
    svg/SVGTextContentElement.idl
    svg/SVGTextElement.idl
    svg/SVGTextPathElement.idl
    svg/SVGTextPositioningElement.idl
    svg/SVGTitleElement.idl
    svg/SVGTransform.idl
    svg/SVGTransformList.idl
    svg/SVGURIReference.idl
    svg/SVGUnitTypes.idl
    svg/SVGUseElement.idl
    svg/SVGVKernElement.idl
    svg/SVGViewElement.idl
    svg/SVGViewSpec.idl
    svg/SVGZoomAndPan.idl
    svg/SVGZoomEvent.idl
)

if (WIN32 AND INTERNAL_BUILD)
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
    set(ANGLE_FORWARDING_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private/ANGLE")
else ()
    set(JavaScriptCore_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts")
    set(ANGLE_FORWARDING_HEADERS_DIR "${FORWARDING_HEADERS_DIR}/ANGLE")
endif ()

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad"
    )

    list(APPEND WebCore_IDL_INCLUDES
        Modules/gamepad
    )

    list(APPEND WebCore_NON_SVG_IDL_FILES
        Modules/gamepad/Gamepad.idl
        Modules/gamepad/GamepadButton.idl
        Modules/gamepad/GamepadEvent.idl
        Modules/gamepad/NavigatorGamepad.idl

        Modules/webvr/GamepadWebVR.idl
    )

    list(APPEND WebCore_SOURCES
        Modules/gamepad/Gamepad.cpp
        Modules/gamepad/GamepadButton.cpp
        Modules/gamepad/GamepadEvent.cpp
        Modules/gamepad/GamepadManager.cpp
        Modules/gamepad/NavigatorGamepad.cpp

        platform/gamepad/EmptyGamepadProvider.cpp
        platform/gamepad/GamepadProvider.cpp
    )
endif ()

set(WebCore_IDL_FILES
    ${WebCore_NON_SVG_IDL_FILES}
    ${WebCore_SVG_IDL_FILES}
)

set(WebCore_CSS_PROPERTY_NAMES
    ${WEBCORE_DIR}/css/CSSProperties.json
)

set(WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/CSSValueKeywords.in
)

set(WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/fullscreen.css
    ${WEBCORE_DIR}/css/html.css
    ${WEBCORE_DIR}/css/mathml.css
    ${WEBCORE_DIR}/css/mediaControls.css
    ${WEBCORE_DIR}/css/plugIns.css
    ${WEBCORE_DIR}/css/quirks.css
    ${WEBCORE_DIR}/css/svg.css
    ${WEBCORE_DIR}/html/shadow/meterElementShadow.css
)

set(WebCore_PLUG_INS_RESOURCES
    ${WEBCORE_DIR}/Resources/plugIns.js
)

set(WebCore_LIBRARIES
    ${ICU_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ZLIB_LIBRARIES}
    PAL${DEBUG_SUFFIX}
)

if (ENABLE_LEGACY_ENCRYPTED_MEDIA)
    list(APPEND WebCore_SOURCES
        Modules/encryptedmedia/legacy/LegacyCDM.cpp
        Modules/encryptedmedia/legacy/LegacyCDMPrivateClearKey.cpp
        Modules/encryptedmedia/legacy/LegacyCDMPrivateMediaPlayer.cpp
        Modules/encryptedmedia/legacy/LegacyCDMSessionClearKey.cpp

        Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeySession.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeys.cpp
    )
    list(APPEND WebCoreTestSupport_SOURCES
        testing/LegacyMockCDM.cpp
    )
    list(APPEND WebCore_IDL_FILES
        Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeySession.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeys.idl

        html/WebKitMediaKeyError.idl
    )
endif ()

if (ENABLE_ENCRYPTED_MEDIA)
    list(APPEND WebCore_SOURCES
        Modules/encryptedmedia/CDM.cpp
        Modules/encryptedmedia/InitDataRegistry.cpp
        Modules/encryptedmedia/MediaKeyMessageEvent.cpp
        Modules/encryptedmedia/MediaKeySession.cpp
        Modules/encryptedmedia/MediaKeyStatusMap.cpp
        Modules/encryptedmedia/MediaKeySystemAccess.cpp
        Modules/encryptedmedia/MediaKeys.cpp
        Modules/encryptedmedia/NavigatorEME.cpp

        html/MediaEncryptedEvent.cpp
    )
    list(APPEND WebCore_IDL_FILES
        Modules/encryptedmedia/MediaKeyMessageEvent.idl
        Modules/encryptedmedia/MediaKeySession.idl
        Modules/encryptedmedia/MediaKeySessionType.idl
        Modules/encryptedmedia/MediaKeyStatusMap.idl
        Modules/encryptedmedia/MediaKeySystemAccess.idl
        Modules/encryptedmedia/MediaKeySystemConfiguration.idl
        Modules/encryptedmedia/MediaKeySystemMediaCapability.idl
        Modules/encryptedmedia/MediaKeys.idl
        Modules/encryptedmedia/MediaKeysRequirement.idl
        Modules/encryptedmedia/NavigatorEME.idl

        html/MediaEncryptedEvent.idl
    )
endif ()

list(APPEND WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/SVGCSSValueKeywords.in
)

if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        html/canvas/ANGLEInstancedArrays.cpp
        html/canvas/EXTBlendMinMax.cpp
        html/canvas/EXTFragDepth.cpp
        html/canvas/EXTShaderTextureLOD.cpp
        html/canvas/EXTTextureFilterAnisotropic.cpp
        html/canvas/EXTsRGB.cpp
        html/canvas/OESElementIndexUint.cpp
        html/canvas/OESStandardDerivatives.cpp
        html/canvas/OESTextureFloat.cpp
        html/canvas/OESTextureFloatLinear.cpp
        html/canvas/OESTextureHalfFloat.cpp
        html/canvas/OESTextureHalfFloatLinear.cpp
        html/canvas/OESVertexArrayObject.cpp
        html/canvas/WebGL2RenderingContext.cpp
        html/canvas/WebGLBuffer.cpp
        html/canvas/WebGLCompressedTextureATC.cpp
        html/canvas/WebGLCompressedTexturePVRTC.cpp
        html/canvas/WebGLCompressedTextureS3TC.cpp
        html/canvas/WebGLContextEvent.cpp
        html/canvas/WebGLContextGroup.cpp
        html/canvas/WebGLContextObject.cpp
        html/canvas/WebGLDebugRendererInfo.cpp
        html/canvas/WebGLDebugShaders.cpp
        html/canvas/WebGLDepthTexture.cpp
        html/canvas/WebGLDrawBuffers.cpp
        html/canvas/WebGLExtension.cpp
        html/canvas/WebGLFramebuffer.cpp
        html/canvas/WebGLLoseContext.cpp
        html/canvas/WebGLObject.cpp
        html/canvas/WebGLProgram.cpp
        html/canvas/WebGLQuery.cpp
        html/canvas/WebGLRenderbuffer.cpp
        html/canvas/WebGLRenderingContext.cpp
        html/canvas/WebGLRenderingContextBase.cpp
        html/canvas/WebGLSampler.cpp
        html/canvas/WebGLShader.cpp
        html/canvas/WebGLShaderPrecisionFormat.cpp
        html/canvas/WebGLSharedObject.cpp
        html/canvas/WebGLSync.cpp
        html/canvas/WebGLTexture.cpp
        html/canvas/WebGLTransformFeedback.cpp
        html/canvas/WebGLUniformLocation.cpp
        html/canvas/WebGLVertexArrayObject.cpp
        html/canvas/WebGLVertexArrayObjectBase.cpp
        html/canvas/WebGLVertexArrayObjectOES.cpp
    )
    list(APPEND WebCore_IDL_FILES
        html/canvas/ANGLEInstancedArrays.idl
        html/canvas/EXTBlendMinMax.idl
        html/canvas/EXTFragDepth.idl
        html/canvas/EXTShaderTextureLOD.idl
        html/canvas/EXTTextureFilterAnisotropic.idl
        html/canvas/EXTsRGB.idl
        html/canvas/OESElementIndexUint.idl
        html/canvas/OESStandardDerivatives.idl
        html/canvas/OESTextureFloat.idl
        html/canvas/OESTextureFloatLinear.idl
        html/canvas/OESTextureHalfFloat.idl
        html/canvas/OESTextureHalfFloatLinear.idl
        html/canvas/OESVertexArrayObject.idl
        html/canvas/WebGL2RenderingContext.idl
        html/canvas/WebGLActiveInfo.idl
        html/canvas/WebGLBuffer.idl
        html/canvas/WebGLCompressedTextureATC.idl
        html/canvas/WebGLCompressedTexturePVRTC.idl
        html/canvas/WebGLCompressedTextureS3TC.idl
        html/canvas/WebGLContextAttributes.idl
        html/canvas/WebGLContextEvent.idl
        html/canvas/WebGLDebugRendererInfo.idl
        html/canvas/WebGLDebugShaders.idl
        html/canvas/WebGLDepthTexture.idl
        html/canvas/WebGLDrawBuffers.idl
        html/canvas/WebGLFramebuffer.idl
        html/canvas/WebGLLoseContext.idl
        html/canvas/WebGLProgram.idl
        html/canvas/WebGLQuery.idl
        html/canvas/WebGLRenderbuffer.idl
        html/canvas/WebGLRenderingContext.idl
        html/canvas/WebGLRenderingContextBase.idl
        html/canvas/WebGLSampler.idl
        html/canvas/WebGLShader.idl
        html/canvas/WebGLShaderPrecisionFormat.idl
        html/canvas/WebGLSync.idl
        html/canvas/WebGLTexture.idl
        html/canvas/WebGLTransformFeedback.idl
        html/canvas/WebGLUniformLocation.idl
        html/canvas/WebGLVertexArrayObject.idl
        html/canvas/WebGLVertexArrayObjectOES.idl
    )
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebCore_IDL_FILES
        html/track/AudioTrack.idl
        html/track/AudioTrackList.idl
        html/track/DataCue.idl
        html/track/TextTrack.idl
        html/track/TextTrackCue.idl
        html/track/TextTrackCueList.idl
        html/track/TextTrackList.idl
        html/track/TrackEvent.idl
        html/track/VTTCue.idl
        html/track/VTTRegion.idl
        html/track/VTTRegionList.idl
        html/track/VideoTrack.idl
        html/track/VideoTrackList.idl
    )

    if (ENABLE_MEDIA_SOURCE)
        list(APPEND WebCore_IDL_FILES
            Modules/mediasource/AudioTrackMediaSource.idl
            Modules/mediasource/TextTrackMediaSource.idl
            Modules/mediasource/VideoTrackMediaSource.idl
        )
    endif ()

    list(APPEND WebCore_SOURCES
        html/track/AudioTrack.cpp
        html/track/AudioTrackList.cpp
        html/track/BufferedLineReader.cpp
        html/track/DataCue.cpp
        html/track/InbandDataTextTrack.cpp
        html/track/InbandGenericTextTrack.cpp
        html/track/InbandTextTrack.cpp
        html/track/InbandWebVTTTextTrack.cpp
        html/track/LoadableTextTrack.cpp
        html/track/TextTrack.cpp
        html/track/TextTrackCue.cpp
        html/track/TextTrackCueGeneric.cpp
        html/track/TextTrackCueList.cpp
        html/track/TextTrackList.cpp
        html/track/TrackBase.cpp
        html/track/TrackEvent.cpp
        html/track/TrackListBase.cpp
        html/track/VTTCue.cpp
        html/track/VTTRegion.cpp
        html/track/VTTRegionList.cpp
        html/track/VTTScanner.cpp
        html/track/VideoTrack.cpp
        html/track/VideoTrackList.cpp
        html/track/WebVTTElement.cpp
        html/track/WebVTTParser.cpp
        html/track/WebVTTTokenizer.cpp

        loader/cache/CachedTextTrack.cpp

        platform/graphics/TextTrackRepresentation.cpp
        platform/graphics/TrackPrivateBase.cpp
    )
endif ()

if (ENABLE_QUOTA)
    list(APPEND WebCore_SOURCES
        Modules/quota/DOMWindowQuota.cpp
        Modules/quota/NavigatorStorageQuota.cpp
        Modules/quota/StorageErrorCallback.cpp
        Modules/quota/StorageInfo.cpp
        Modules/quota/StorageQuota.cpp
        Modules/quota/WorkerNavigatorStorageQuota.cpp
    )

    list(APPEND WebCore_IDL_FILES
        Modules/quota/DOMWindowQuota.idl
        Modules/quota/NavigatorStorageQuota.idl
        Modules/quota/StorageErrorCallback.idl
        Modules/quota/StorageInfo.idl
        Modules/quota/StorageQuota.idl
        Modules/quota/StorageQuotaCallback.idl
        Modules/quota/StorageUsageCallback.idl
        Modules/quota/WorkerNavigatorStorageQuota.idl
    )
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D)
    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_LIBEPOXY)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${LIBEPOXY_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${LIBEPOXY_LIBRARIES}
        )
        add_definitions(${LIBEPOXY_DEFINITIONS})
    else ()
        if (USE_OPENGL)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGL_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${OPENGL_LIBRARIES}
            )
            add_definitions(${OPENGL_DEFINITIONS})
        elseif (USE_OPENGL_ES_2)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGLES2_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${OPENGLES2_LIBRARIES}
            )
            add_definitions(${OPENGLES2_DEFINITIONS})
        endif ()

        if (USE_EGL)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${EGL_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${EGL_LIBRARIES}
            )
            add_definitions(${EGL_DEFINITIONS})
        endif ()
    endif ()

    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${ANGLE_FORWARDING_HEADERS_DIR}"
        "${WEBCORE_DIR}/platform/graphics/gpu"
    )

    list(APPEND ANGLESupport_SOURCES
        ${THIRDPARTY_DIR}/ANGLE/src/common/angleutils.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/debug.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/event_tracer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/Float16ToFloat32.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/mathutil.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/MemoryBuffer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/string_utils.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/third_party/smhasher/src/PMurHash.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/tls.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/uniform_type_info_autogen.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/utilities.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DiagnosticsBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveHandlerBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/ExpressionParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Input.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Lexer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Macro.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/MacroExpander.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Preprocessor.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Token.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Tokenizer.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ASTMetadataHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/AddAndTrueToLoopCondition.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/AddDefaultReturnStatements.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ArrayReturnValueToOutParameter.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BreakVariableAliasingInInnerLoops.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Cache.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CallDAG.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ClampPointSize.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CodeGen.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CollectVariables.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Compiler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ConstantUnion.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DeclareAndInitBuiltinsForInstancedMultiview.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DeferGlobalInitializers.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Diagnostics.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DirectiveHandler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/EmulateGLFragColorBroadcast.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/EmulatePrecision.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ExpandIntegerPowExpressions.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ExtensionBehavior.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ExtensionGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/FindMain.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/FindSymbolNode.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/FlagStd140Structs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/HashNames.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ImageFunctionHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InfoSink.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Initialize.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeDll.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeVariables.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermNode.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermNodePatternMatcher.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermNode_util.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermTraverse.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IsASTDepthBelowLimit.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Operator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSLBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputTree.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputVulkanGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ParseContext.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PoolAlloc.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PruneNoOps.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/QualifierTypes.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RecordConstantPrecision.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RegenerateStructNames.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveArrayLengthMethod.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveDynamicIndexing.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveEmptySwitchStatements.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveInvariantDeclaration.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveNoOpCasesFromEndOfSwitchStatements.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemovePow.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveSwitchFallThrough.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveUnreferencedVariables.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteDoWhile.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteElseBlocks.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteTexelFetchOffset.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteUnaryMinusOperatorFloat.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteUnaryMinusOperatorInt.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RunAtTheEndOfShader.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ScalarizeVecAndMatConstructorArgs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SearchSymbol.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateArrayInitialization.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateDeclarations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateExpressionsReturningArrays.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderLang.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderVars.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SimplifyLoopConditions.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SplitSequenceOperator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/StructureHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SymbolTable.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SymbolUniqueId.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TextureFunctionHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorVulkan.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Types.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuitAST.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuitToIf.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UniformHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UseInterfaceBlockFields.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UtilsHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateGlobalInitializer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateLimitations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateMaxParameters.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateOutputs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateSwitch.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateVaryingLocations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VariablePacker.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VectorizeVectorScalarArithmetic.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VersionGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/WrapSwitchStatementsInBlocks.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayout.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayoutHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/emulated_builtin_functions_hlsl_autogen.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_lex.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_tab.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/util.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/third_party/compiler/ArrayBoundsClamper.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/libANGLE/Platform.cpp
    )

    list(APPEND WebCore_SOURCES
        platform/graphics/ANGLEWebKitBridge.cpp
        platform/graphics/FormatConverter.cpp
        platform/graphics/GraphicsContext3D.cpp
    )
endif ()

if (ENABLE_USER_MESSAGE_HANDLERS)
    list(APPEND WebCore_IDL_FILES
        page/UserMessageHandler.idl
        page/UserMessageHandlersNamespace.idl
        page/WebKitNamespace.idl
    )
    list(APPEND WebCore_SOURCES
        page/UserMessageHandler.cpp
        page/UserMessageHandlerDescriptor.cpp
        page/UserMessageHandlersNamespace.cpp
        page/WebKitNamespace.cpp
    )
endif ()

if (USE_OPENVR)
  list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/openvr/headers")
  list(APPEND WebCore_LIBRARIES openvr_api)
endif ()

if (USE_WOFF2)
  list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${WOFF2DEC_INCLUDE_DIRS}")
  list(APPEND WebCore_LIBRARIES "${WOFF2DEC_LIBRARIES}")
endif ()

if (USE_XDGMIME)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/xdgmime/src")
    list(APPEND WebCore_LIBRARIES xdgmime)
endif ()

set(WebCoreTestSupport_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/testing"
    "${WEBCORE_DIR}/testing/js"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
)

set(WebCoreTestSupport_IDL_INCLUDES
    testing
)

set(WebCoreTestSupport_IDL_FILES
    testing/GCObservation.idl
    testing/InternalSettings.idl
    testing/Internals.idl
    testing/MallocStatistics.idl
    testing/MemoryInfo.idl
    testing/MockCDMFactory.idl
    testing/MockContentFilterSettings.idl
    testing/MockPageOverlay.idl
    testing/ServiceWorkerInternals.idl
    testing/TypeConversions.idl
)

list(APPEND WebCoreTestSupport_SOURCES
    platform/mock/PlatformSpeechSynthesizerMock.cpp

    platform/mock/mediasource/MockBox.cpp
    platform/mock/mediasource/MockMediaPlayerMediaSource.cpp
    platform/mock/mediasource/MockMediaSourcePrivate.cpp
    platform/mock/mediasource/MockSourceBufferPrivate.cpp

    testing/GCObservation.cpp
    testing/InternalSettings.cpp
    testing/Internals.cpp
    testing/MockCDMFactory.cpp
    testing/MockGamepad.cpp
    testing/MockGamepadProvider.cpp
    testing/MockPageOverlay.cpp
    testing/MockPageOverlayClient.cpp
    testing/ServiceWorkerInternals.cpp
    testing/js/WebCoreTestSupport.cpp
)

set(WebCoreTestSupport_LIBRARIES
    PRIVATE JavaScriptCore${DEBUG_SUFFIX}
)

# Modules that the bindings generator scripts may use
set(SCRIPTS_RESOLVE_SUPPLEMENTAL
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
)
set(SCRIPTS_BINDINGS
    ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
    ${WEBCORE_DIR}/bindings/scripts/CodeGeneratorJS.pm
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
    ${WEBCORE_DIR}/bindings/scripts/InFilesParser.pm
    ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
)

set(MAKE_NAMES_DEPENDENCIES
    ${WEBCORE_DIR}/bindings/scripts/Hasher.pm
    ${WEBCORE_DIR}/bindings/scripts/StaticString.pm
)
set(SUPPLEMENTAL_DEPENDENCY_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/supplemental_dependency.tmp)
set(WINDOW_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowConstructors.idl)
set(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/WorkerGlobalScopeConstructors.idl)
set(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DedicatedWorkerGlobalScopeConstructors.idl)
set(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/ServiceWorkerGlobalScopeConstructors.idl)

WEBKIT_FRAMEWORK_DECLARE(WebCore)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# Generate InspectorOverlayPage.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html
    MAIN_DEPENDENCY inspector/InspectorOverlayPage.html
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/inline-and-minify-stylesheets-and-scripts.py
    DEPENDS inspector/InspectorOverlayPage.css inspector/InspectorOverlayPage.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/inline-and-minify-stylesheets-and-scripts.py ${WEBCORE_DIR}/inspector/InspectorOverlayPage.html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl InspectorOverlayPage_html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h)


# Generate CommandLineAPIModuleSource.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    MAIN_DEPENDENCY inspector/CommandLineAPIModuleSource.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js" > ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/inspector/CommandLineAPIModuleSource.js >> ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl CommandLineAPIModuleSource_js ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h)


MAKE_HASH_TOOLS(${WEBCORE_DIR}/platform/ColorData)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/ColorData.cpp)


# Generate DOMJITAbstractHeapRepository.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h
    MAIN_DEPENDENCY domjit/DOMJITAbstractHeapRepository.yaml
    DEPENDS ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb
    COMMAND ${RUBY_EXECUTABLE} ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb ${WEBCORE_DIR}/domjit/DOMJITAbstractHeapRepository.yaml ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h)

# Generate XMLViewerCSS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    MAIN_DEPENDENCY xml/XMLViewer.css
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py < ${WEBCORE_DIR}/xml/XMLViewer.css > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h)

# Generate XMLViewerJS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    MAIN_DEPENDENCY xml/XMLViewer.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/xml/XMLViewer.js > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h)

# Generate HTML entity table
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    DEPENDS ${WEBCORE_DIR}/html/parser/create-html-entity-table
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/html/parser/create-html-entity-table -o ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp)

# Generate HTTP header names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in
    DEPENDS ${WEBCORE_DIR}/platform/network/create-http-header-name-table
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/platform/network/create-http-header-name-table ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp)

# Generate CSS property names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSProperties.json ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.h ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeprop.pl
    DEPENDS ${WebCore_CSS_PROPERTY_NAMES}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_PROPERTY_NAMES} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSProperties.json
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makeprop.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --gperf-executable "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)

# Generate CSS value keywords
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.gperf
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makevalues.pl
    DEPENDS ${WebCore_CSS_VALUE_KEYWORDS}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_VALUE_KEYWORDS} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makevalues.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSPrimitiveValueMappings.h CSSValueKeywords.h)

# Generate CSS Selector pseudo type name to value maps.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp)

# Generate user agent styles
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_USER_AGENT_STYLE_SHEETS} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${WebCore_USER_AGENT_STYLE_SHEETS}
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp UserAgentStyleSheetsData.cpp UserAgentStyleSheets.h)

if (WebCore_USER_AGENT_SCRIPTS)
    # Necessary variables:
    # WebCore_USER_AGENT_SCRIPTS containing the JavaScript sources list
    # WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES containing the source file that will load the scripts to add the proper
    #   dependency and having them built at the right moment

    MAKE_JS_FILE_ARRAYS(
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScriptsData.cpp
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.h
        WebCore
        WebCore_USER_AGENT_SCRIPTS
        WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES
    )
    list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScriptsData.cpp)
endif ()

# Generate plug-in resources
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResources.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_PLUG_INS_RESOURCES} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResources.h ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp ${WebCore_PLUG_INS_RESOURCES}
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp PlugInsResourcesData.cpp PlugInsResources.h)

set(FEATURE_DEFINES_JAVASCRIPT "LANGUAGE_JAVASCRIPT ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
list(APPEND WebCoreTestSupport_IDL_FILES ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.idl)

GENERATE_BINDINGS(WebCoreBindings
    OUTPUT_SOURCE WebCore_SOURCES
    INPUT_FILES ${WebCore_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${DERIVED_SOURCES_WEBCORE_DIR}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${DERIVED_SOURCES_WEBCORE_DIR}
    GENERATOR JS
    SUPPLEMENTAL_DEPFILE ${SUPPLEMENTAL_DEPENDENCY_FILE}
    PP_EXTRA_OUTPUT
        ${WINDOW_CONSTRUCTORS_FILE} ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE} ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    PP_EXTRA_ARGS
        --windowConstructorsFile ${WINDOW_CONSTRUCTORS_FILE}
        --workerGlobalScopeConstructorsFile ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --dedicatedWorkerGlobalScopeConstructorsFile ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --serviceWorkerGlobalScopeConstructorsFile ${SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE})

GENERATE_BINDINGS(WebCoreTestSupportBindings
    OUTPUT_SOURCE WebCoreTestSupport_SOURCES
    INPUT_FILES ${WebCoreTestSupport_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${DERIVED_SOURCES_WEBCORE_DIR} ${WebCoreTestSupport_IDL_INCLUDES}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${DERIVED_SOURCES_WEBCORE_DIR}
    GENERATOR JS)

# WebCoreTestSupportBindings needs InternalSettingsGenerated.idl
add_dependencies(WebCoreTestSupportBindings WebCore)

# WebCore JS Builtins

set(WebCore_BUILTINS_SOURCES
    ${WEBCORE_DIR}/Modules/mediastream/NavigatorUserMedia.js
    ${WEBCORE_DIR}/Modules/mediastream/RTCPeerConnection.js
    ${WEBCORE_DIR}/Modules/mediastream/RTCPeerConnectionInternals.js
    ${WEBCORE_DIR}/Modules/streams/ByteLengthQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/CountQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStream.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamBYOBReader.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamBYOBRequest.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultReader.js
    ${WEBCORE_DIR}/Modules/streams/StreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/WritableStream.js
    ${WEBCORE_DIR}/Modules/streams/WritableStreamInternals.js
    ${WEBCORE_DIR}/bindings/js/JSDOMBindingInternals.js
)

set(BUILTINS_GENERATOR_SCRIPTS
    ${JavaScriptCore_SCRIPTS_DIR}/builtins.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generator.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_model.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_templates.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py
    ${JavaScriptCore_SCRIPTS_DIR}/lazywriter.py
)

foreach (_builtinSource ${WebCore_BUILTINS_SOURCES})
    get_filename_component(_objectName ${_builtinSource} NAME_WE)
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.cpp
               ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.h
        MAIN_DEPENDENCY ${_builtinSource}
        DEPENDS ${BUILTINS_GENERATOR_SCRIPTS}
        COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --framework WebCore --output-directory ${DERIVED_SOURCES_WEBCORE_DIR} ${_builtinSource}
        VERBATIM)
    list(APPEND WebCore_DERIVED_BUILTIN_HEADERS ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.h)
endforeach ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.cpp
           ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.cpp
           ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.h
           ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.h
    MAIN_DEPENDENCY ${WebCore_BUILTINS_SOURCES}
    DEPENDS ${BUILTINS_GENERATOR_SCRIPTS} ${WebCore_DERIVED_BUILTIN_HEADERS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --wrappers-only --framework WebCore --output-directory ${DERIVED_SOURCES_WEBCORE_DIR} ${WebCore_BUILTINS_SOURCES}
    VERBATIM)
list(APPEND WebCore_SOURCES
    ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.cpp
    ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.cpp
    ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.h
    ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.h)

ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/html/HTMLTreeBuilder.cpp MathMLNames.cpp)


GENERATE_DOM_NAMES(HTML ${WEBCORE_DIR}/html/HTMLAttributeNames.in ${WEBCORE_DIR}/html/HTMLTagNames.in "")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JSHTMLElementWrapperFactory.cpp)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventNames.in EventInterfaces.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventInterfaces.h)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventTargetFactory.in EventTargetInterfaces.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventTargetInterfaces.h)


GENERATE_SETTINGS_MACROS(${WEBCORE_DIR}/page/Settings.yaml Settings.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/Settings.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/Settings.cpp)
list(APPEND WebCoreTestSupport_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.cpp)


GENERATE_FONT_NAMES(${WEBCORE_DIR}/css/WebKitFontFamilyNames.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.cpp)


GENERATE_DOM_NAMES(MathML ${WEBCORE_DIR}/mathml/mathattrs.in ${WEBCORE_DIR}/mathml/mathtags.in "")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLNames.cpp)
if (ENABLE_MATHML)
    list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLElementFactory.cpp)
endif ()

# SVG extra defines need to map to a numerical value for correct preprocessing of svgtags.in.
set(SVG_EXTRA_DEFINES "")
if (ENABLE_SVG_FONTS)
    list(APPEND SVG_EXTRA_DEFINES "ENABLE_SVG_FONTS=1")
endif ()

GENERATE_DOM_NAMES(SVG ${WEBCORE_DIR}/svg/svgattrs.in ${WEBCORE_DIR}/svg/svgtags.in "${SVG_EXTRA_DEFINES}")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGNames.cpp)

list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JSSVGElementWrapperFactory.cpp)

GENERATE_DOM_NAMES(XLink ${WEBCORE_DIR}/svg/xlinkattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XLinkNames.cpp)


GENERATE_DOM_NAMES(XMLNS ${WEBCORE_DIR}/xml/xmlnsattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNSNames.cpp)


GENERATE_DOM_NAMES(XML ${WEBCORE_DIR}/xml/xmlattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNames.cpp)

WEBKIT_WRAP_SOURCELIST(${WebCore_IDL_FILES})
WEBKIT_COMPUTE_SOURCES(WebCore)
WEBKIT_WRAP_SOURCELIST(${WebCore_SOURCES})
WEBKIT_WRAP_SOURCELIST(${WebCoreTestSupport_IDL_FILES} ${WebCoreTestSupport_SOURCES})

if (MSVC)
    WEBKIT_ADD_PRECOMPILED_HEADER("WebCorePrefix.h" "WebCorePrefix.cpp" WebCore_SOURCES)
endif ()

WEBKIT_FRAMEWORK(WebCore)

WEBKIT_MAKE_FORWARDING_HEADERS(WebCore
    FILES ${WebCore_PRIVATE_FRAMEWORK_HEADERS}
    DERIVED_SOURCE_DIRECTORIES ${DERIVED_SOURCES_WEBCORE_DIR} ${DERIVED_SOURCES_PAL_DIR}
    FLATTENED
)

add_dependencies(WebCore WebCoreBindings)

if (APPLE)
    set_target_properties(WebCore PROPERTIES LINK_FLAGS "-sub_library libobjc -umbrella WebKit -allowable_client WebCoreTestSupport -allowable_client WebKit2 -allowable_client WebKitLegacy")
endif ()

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebCore -fno-tree-sra)
endif ()

if (MSVC)
    WEBKIT_ADD_PRECOMPILED_HEADER("WebCoreTestSupportPrefix.h" "testing/js/WebCoreTestSupportPrefix.cpp" WebCoreTestSupport_SOURCES)
endif ()

add_library(WebCoreTestSupport ${WebCoreTestSupport_LIBRARY_TYPE} ${WebCoreTestSupport_SOURCES})
add_dependencies(WebCoreTestSupport WebCore WebCoreTestSupportBindings)
target_include_directories(WebCoreTestSupport PRIVATE ${WebCoreTestSupport_PRIVATE_INCLUDE_DIRECTORIES} ${WebCore_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WebCoreTestSupport SYSTEM PUBLIC ${WebCore_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WebCoreTestSupport ${WebCoreTestSupport_LIBRARIES})

if (WebCoreTestSupport_OUTPUT_NAME)
    set_target_properties(WebCoreTestSupport PROPERTIES OUTPUT_NAME ${WebCoreTestSupport_OUTPUT_NAME})
    if (MSVC)
        add_compile_options(/GF- /WX-)
    endif ()
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D AND NOT WIN32)
    add_library(ANGLESupport STATIC ${ANGLESupport_SOURCES})

    # Enable the ESSL and GLSL translators.
    set_property(TARGET ANGLESupport
        PROPERTY COMPILE_DEFINITIONS
        ANGLE_ENABLE_ESSL
        ANGLE_ENABLE_GLSL
        ${ANGLE_PLATFORM_DEFINITIONS})

    target_include_directories(ANGLESupport PRIVATE
        "${THIRDPARTY_DIR}/ANGLE"
        "${THIRDPARTY_DIR}/ANGLE/include"
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
        "${THIRDPARTY_DIR}/ANGLE/src"
        "${THIRDPARTY_DIR}/ANGLE/src/common/third_party/base"
    )
    list(APPEND WebCore_LIBRARIES ANGLESupport)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(ANGLESupport -Wno-implicit-fallthrough
                                                 -Wno-inconsistent-missing-override
                                                 -Wno-null-conversion
                                                 -Wno-suggest-attribute=format
                                                 -Wno-unused-function
                                                 -Wno-unused-parameter)
    endif ()
endif ()

target_link_libraries(WebCore ${WebCore_LIBRARIES})

if (${WebCore_LIBRARY_TYPE} MATCHES "SHARED")
    set_target_properties(WebCore PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS WebCore DESTINATION "${LIB_INSTALL_DIR}")
endif ()

if (MSVC)
    set_target_properties(WebCore PROPERTIES
        COMPILE_PDB_NAME ${WebCore_OUTPUT_NAME}
    )
endif ()

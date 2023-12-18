/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(SERVICEEXTENSIONS) && defined(__OBJC__) && __OBJC__
#import <ServiceExtensions/ServiceExtensions.h>
#import <ServiceExtensions/ServiceExtensions_Private.h>
#endif

#if USE(APPLE_INTERNAL_SDK)

#import <WebKitAdditions/WKSEDefinitionsAdditions.h>

#else

#define WKSEScrollViewDelegate UIScrollViewDelegate
#define WKSEScrollView UIScrollView

#endif

#ifndef SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
#define SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE 0
#define WKSETextSuggestion                                      UITextSuggestion
#define WKSEAutoFillTextSuggestion                              UITextAutofillSuggestion
#define WKSEKeyEvent                                            UIKeyEvent
#define WKSEKeyEventContext                                     UIKeyEventContext
#define WKSETextDocumentContext                                 UIWKDocumentContext
#define WKSETextDocumentRequest                                 UIWKDocumentRequest
#define WKSETextDocumentRequestFlags                            UIWKDocumentRequestFlags
#define WKSETextDocumentRequestNone                             UIWKDocumentRequestNone
#define WKSETextDocumentRequestText                             UIWKDocumentRequestText
#define WKSETextDocumentRequestAttributed                       UIWKDocumentRequestAttributed
#define WKSETextDocumentRequestRects                            UIWKDocumentRequestRects
#define WKSETextDocumentRequestSpatial                          UIWKDocumentRequestSpatial
#define WKSETextDocumentRequestAnnotation                       UIWKDocumentRequestAnnotation
#define WKSETextDocumentRequestMarkedTextRects                  UIWKDocumentRequestMarkedTextRects
#define WKSETextDocumentRequestSpatialAndCurrentSelection       UIWKDocumentRequestSpatialAndCurrentSelection
#define WKSETextDocumentRequestAutocorrectedRanges              UIWKDocumentRequestAutocorrectedRanges
#define WKSEDirectionalTextRange                                UIDirectionalTextRange
#define WKSETextReplacementOptions                              UITextReplacementOptions
#define WKSETextReplacementOptionsNone                          UITextReplacementOptionsNone
#define WKSETextReplacementOptionsAddUnderline                  UITextReplacementOptionsAddUnderline
#define WKSEShiftKeyState                                       UIShiftKeyState
#define WKSEShiftKeyStateNone                                   UIShiftKeyStateNone
#define WKSEShiftKeyStateShifted                                UIShiftKeyStateShifted
#define WKSEShiftKeyStateCapsLocked                             UIShiftKeyStateCapsLocked
#define WKSEExtendedTextInputTraits                             UIExtendedTextInputTraits
#define WKSETextInput                                           UIAsyncTextInputClient
#define WKSETextInputDelegate                                   UIAsyncTextInputDelegate
#define WKSETextInteraction                                     UIAsyncTextInteraction
#define WKSETextInteractionDelegate                             UIAsyncTextInteractionDelegate
#define WKSEGestureType                                         UIWKGestureType
#define WKSEGestureLoupe                                        UIWKGestureLoupe
#define WKSEGestureOneFingerTap                                 UIWKGestureOneFingerTap
#define WKSEGestureTapAndAHalf                                  UIWKGestureTapAndAHalf
#define WKSEGestureDoubleTap                                    UIWKGestureDoubleTap
#define WKSEGestureOneFingerDoubleTap                           UIWKGestureOneFingerDoubleTap
#define WKSEGestureOneFingerTripleTap                           UIWKGestureOneFingerTripleTap
#define WKSEGestureTwoFingerSingleTap                           UIWKGestureTwoFingerSingleTap
#define WKSEGestureTwoFingerRangedSelectGesture                 UIWKGestureTwoFingerRangedSelectGesture
#define WKSEGesturePhraseBoundary                               UIWKGesturePhraseBoundary
#define WKSESelectionTouch                                      UIWKSelectionTouch
#define WKSESelectionTouchStarted                               UIWKSelectionTouchStarted
#define WKSESelectionTouchMoved                                 UIWKSelectionTouchMoved
#define WKSESelectionTouchEnded                                 UIWKSelectionTouchEnded
#define WKSESelectionTouchEndedMovingForward                    UIWKSelectionTouchEndedMovingForward
#define WKSESelectionTouchEndedMovingBackward                   UIWKSelectionTouchEndedMovingBackward
#define WKSESelectionTouchEndedNotMoving                        UIWKSelectionTouchEndedNotMoving
#define WKSESelectionFlags                                      UIWKSelectionFlags
#define WKSESelectionFlagsNone                                  UIWKNone
#define WKSEWordIsNearTap                                       UIWKWordIsNearTap
#define WKSESelectionFlipped                                    UIWKSelectionFlipped
#define WKSEPhraseBoundaryChanged                               UIWKPhraseBoundaryChanged
#endif // !defined(SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE)

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
#endif

#ifndef SERVICE_EXTENSIONS_SCROLL_VIEW_IS_AVAILABLE
#define SERVICE_EXTENSIONS_SCROLL_VIEW_IS_AVAILABLE 0
#define WKSEScrollView                              UIScrollView
#define WKSEScrollViewDelegate                      UIScrollViewDelegate
#define WKSEScrollViewScrollUpdate                  UIScrollEvent
#define WKSEScrollViewScrollUpdatePhase             UIScrollPhase
#define WKSEScrollViewScrollUpdatePhaseBegan        UIScrollPhaseBegan
#define WKSEScrollViewScrollUpdatePhaseChanged      UIScrollPhaseChanged
#define WKSEScrollViewScrollUpdatePhaseEnded        UIScrollPhaseEnded
#define WKSEScrollViewScrollUpdatePhaseCancelled    UIScrollPhaseCancelled
#endif

#ifndef SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
#define SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE 0
#define WKSETextSuggestion                                      UITextSuggestion
#define WKSEAutoFillTextSuggestion                              UITextAutofillSuggestion
#define WKSEKeyEntry                                            UIKeyEvent
#define WKSEKeyEntryContext                                     UIKeyEventContext
#define WKSETextDocumentContext                                 UIWKDocumentContext
#define WKSETextDocumentRequest                                 UIWKDocumentRequest
#define WKSETextDocumentRequestOptions                          UIWKDocumentRequestFlags
#define WKSETextDocumentRequestOptionNone                       UIWKDocumentRequestNone
#define WKSETextDocumentRequestOptionText                       UIWKDocumentRequestText
#define WKSETextDocumentRequestOptionAttributedText             UIWKDocumentRequestAttributed
#define WKSETextDocumentRequestOptionTextRects                  UIWKDocumentRequestRects
#define WKSETextDocumentRequestOptionMarkedTextRects            UIWKDocumentRequestMarkedTextRects
#define WKSETextDocumentRequestOptionAutocorrectedRanges        UIWKDocumentRequestAutocorrectedRanges
#define WKSEDirectionalTextRange                                UIDirectionalTextRange
#define WKSETextReplacementOptions                              UITextReplacementOptions
#define WKSETextReplacementOptionsNone                          UITextReplacementOptionsNone
#define WKSETextReplacementOptionsAddUnderline                  UITextReplacementOptionsAddUnderline
#define WKSEKeyModifierFlags                                    UIShiftKeyState
#define WKSEKeyModifierFlagsNone                                UIShiftKeyStateNone
#define WKSEKeyModifierFlagsShifted                             UIShiftKeyStateShifted
#define WKSEKeyModifierFlagsCapsLock                            UIShiftKeyStateCapsLocked
#define WKSEExtendedTextInputTraits                             UIExtendedTextInputTraits
#define WKSETextInput                                           UIAsyncTextInputClient
#define WKSETextInputDelegate                                   UIAsyncTextInputDelegate
#define WKSETextInteraction                                     UIAsyncTextInteraction
#define WKSETextInteractionDelegate                             UIAsyncTextInteractionDelegate
#define WKSEGestureType                                         UIWKGestureType
#define WKSEGestureTypeLoupe                                    UIWKGestureLoupe
#define WKSEGestureTypeOneFingerTap                             UIWKGestureOneFingerTap
#define WKSEGestureTypeDoubleTapAndHold                         UIWKGestureTapAndAHalf
#define WKSEGestureTypeDoubleTap                                UIWKGestureDoubleTap
#define WKSEGestureTypeOneFingerDoubleTap                       UIWKGestureOneFingerDoubleTap
#define WKSEGestureTypeOneFingerTripleTap                       UIWKGestureOneFingerTripleTap
#define WKSEGestureTypeTwoFingerSingleTap                       UIWKGestureTwoFingerSingleTap
#define WKSEGestureTypeTwoFingerRangedSelectGesture             UIWKGestureTwoFingerRangedSelectGesture
#define WKSEGestureTypeIMPhraseBoundaryDrag                     UIWKGesturePhraseBoundary
#define WKSESelectionTouchPhase                                 UIWKSelectionTouch
#define WKSESelectionTouchPhaseStarted                          UIWKSelectionTouchStarted
#define WKSESelectionTouchPhaseMoved                            UIWKSelectionTouchMoved
#define WKSESelectionTouchPhaseEnded                            UIWKSelectionTouchEnded
#define WKSESelectionTouchPhaseEndedMovingForward               UIWKSelectionTouchEndedMovingForward
#define WKSESelectionTouchPhaseEndedMovingBackward              UIWKSelectionTouchEndedMovingBackward
#define WKSESelectionTouchPhaseEndedNotMoving                   UIWKSelectionTouchEndedNotMoving
#define WKSESelectionFlags                                      UIWKSelectionFlags
#define WKSESelectionFlagsNone                                  UIWKNone
#define WKSEWordIsNearTap                                       UIWKWordIsNearTap
#define WKSESelectionFlipped                                    UIWKSelectionFlipped
#define WKSEPhraseBoundaryChanged                               UIWKPhraseBoundaryChanged
#endif // !defined(SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE)

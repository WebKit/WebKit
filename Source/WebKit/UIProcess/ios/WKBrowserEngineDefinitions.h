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

#if defined(__OBJC__) && __OBJC__
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#endif

#if USE(BROWSERENGINEKIT)
// Scroll view
#define WKBEScrollView                                          BEScrollView
#define WKBEScrollViewDelegate                                  BEScrollViewDelegate
#define WKBEScrollViewScrollUpdate                              BEScrollViewScrollUpdate
#define WKBEScrollViewScrollUpdatePhase                         BEScrollViewScrollUpdatePhase
#define WKBEScrollViewScrollUpdatePhaseBegan                    BEScrollViewScrollUpdatePhaseBegan
#define WKBEScrollViewScrollUpdatePhaseChanged                  BEScrollViewScrollUpdatePhaseChanged
#define WKBEScrollViewScrollUpdatePhaseEnded                    BEScrollViewScrollUpdatePhaseEnded
#define WKBEScrollViewScrollUpdatePhaseCancelled                BEScrollViewScrollUpdatePhaseCancelled
// Editing and keyboards
#define WKBETextSuggestion                                      BETextSuggestion
#define WKBETextDocumentContext                                 BETextDocumentContext
#define WKBETextDocumentRequest                                 BETextDocumentRequest
#define WKBETextDocumentRequestOptions                          BETextDocumentRequestOptions
#define WKBETextDocumentRequestOptionNone                       BETextDocumentOptionNone
#define WKBETextDocumentRequestOptionText                       BETextDocumentOptionText
#define WKBETextDocumentRequestOptionAttributedText             BETextDocumentOptionAttributedText
#define WKBETextDocumentRequestOptionTextRects                  BETextDocumentOptionTextRects
#define WKBETextDocumentRequestOptionMarkedTextRects            BETextDocumentOptionMarkedTextRects
#define WKBETextDocumentRequestOptionAutocorrectedRanges        BETextDocumentOptionAutocorrectedRanges
#define WKBEGestureType                                         BEGestureType
#define WKBEGestureTypeLoupe                                    BEGestureTypeLoupe
#define WKBEGestureTypeOneFingerTap                             BEGestureTypeOneFingerTap
#define WKBEGestureTypeDoubleTapAndHold                         BEGestureTypeDoubleTapAndHold
#define WKBEGestureTypeDoubleTap                                BEGestureTypeDoubleTap
#define WKBEGestureTypeOneFingerDoubleTap                       BEGestureTypeOneFingerDoubleTap
#define WKBEGestureTypeOneFingerTripleTap                       BEGestureTypeOneFingerTripleTap
#define WKBEGestureTypeTwoFingerSingleTap                       BEGestureTypeTwoFingerSingleTap
#define WKBEGestureTypeTwoFingerRangedSelectGesture             BEGestureTypeTwoFingerRangedSelectGesture
#define WKBEGestureTypeIMPhraseBoundaryDrag                     BEGestureTypeIMPhraseBoundaryDrag
#define WKBEGestureTypeForceTouch                               BEGestureTypeForceTouch
#define WKBESelectionTouchPhase                                 BESelectionTouchPhase
#define WKBESelectionTouchPhaseStarted                          BESelectionTouchPhaseStarted
#define WKBESelectionTouchPhaseMoved                            BESelectionTouchPhaseMoved
#define WKBESelectionTouchPhaseEnded                            BESelectionTouchPhaseEnded
#define WKBESelectionTouchPhaseEndedMovingForward               BESelectionTouchPhaseEndedMovingForward
#define WKBESelectionTouchPhaseEndedMovingBackward              BESelectionTouchPhaseEndedMovingBackward
#define WKBESelectionTouchPhaseEndedNotMoving                   BESelectionTouchPhaseEndedNotMoving
#define WKBESelectionFlags                                      BESelectionFlags
#define WKBESelectionFlagsNone                                  BESelectionFlagsNone
#define WKBEWordIsNearTap                                       BEWordIsNearTap
#define WKBESelectionFlipped                                    BESelectionFlipped
#define WKBEPhraseBoundaryChanged                               BEPhraseBoundaryChanged
#else
// Scroll view
#define WKBEScrollView                                          UIScrollView
#define WKBEScrollViewDelegate                                  UIScrollViewDelegate
#define WKBEScrollViewScrollUpdate                              UIScrollEvent
#define WKBEScrollViewScrollUpdatePhase                         UIScrollPhase
#define WKBEScrollViewScrollUpdatePhaseBegan                    UIScrollPhaseBegan
#define WKBEScrollViewScrollUpdatePhaseChanged                  UIScrollPhaseChanged
#define WKBEScrollViewScrollUpdatePhaseEnded                    UIScrollPhaseEnded
#define WKBEScrollViewScrollUpdatePhaseCancelled                UIScrollPhaseCancelled
// Editing and keyboards
#define WKBETextSuggestion                                      UITextSuggestion
#define WKBETextDocumentContext                                 UIWKDocumentContext
#define WKBETextDocumentRequest                                 UIWKDocumentRequest
#define WKBETextDocumentRequestOptions                          UIWKDocumentRequestFlags
#define WKBETextDocumentRequestOptionNone                       UIWKDocumentRequestNone
#define WKBETextDocumentRequestOptionText                       UIWKDocumentRequestText
#define WKBETextDocumentRequestOptionAttributedText             UIWKDocumentRequestAttributed
#define WKBETextDocumentRequestOptionTextRects                  UIWKDocumentRequestRects
#define WKBETextDocumentRequestOptionMarkedTextRects            UIWKDocumentRequestMarkedTextRects
#define WKBETextDocumentRequestOptionAutocorrectedRanges        UIWKDocumentRequestAutocorrectedRanges
#define WKBEGestureType                                         UIWKGestureType
#define WKBEGestureTypeLoupe                                    UIWKGestureLoupe
#define WKBEGestureTypeOneFingerTap                             UIWKGestureOneFingerTap
#define WKBEGestureTypeDoubleTapAndHold                         UIWKGestureTapAndAHalf
#define WKBEGestureTypeDoubleTap                                UIWKGestureDoubleTap
#define WKBEGestureTypeOneFingerDoubleTap                       UIWKGestureOneFingerDoubleTap
#define WKBEGestureTypeOneFingerTripleTap                       UIWKGestureOneFingerTripleTap
#define WKBEGestureTypeTwoFingerSingleTap                       UIWKGestureTwoFingerSingleTap
#define WKBEGestureTypeTwoFingerRangedSelectGesture             UIWKGestureTwoFingerRangedSelectGesture
#define WKBEGestureTypeIMPhraseBoundaryDrag                     UIWKGesturePhraseBoundary
#define WKBESelectionTouchPhase                                 UIWKSelectionTouch
#define WKBESelectionTouchPhaseStarted                          UIWKSelectionTouchStarted
#define WKBESelectionTouchPhaseMoved                            UIWKSelectionTouchMoved
#define WKBESelectionTouchPhaseEnded                            UIWKSelectionTouchEnded
#define WKBESelectionTouchPhaseEndedMovingForward               UIWKSelectionTouchEndedMovingForward
#define WKBESelectionTouchPhaseEndedMovingBackward              UIWKSelectionTouchEndedMovingBackward
#define WKBESelectionTouchPhaseEndedNotMoving                   UIWKSelectionTouchEndedNotMoving
#define WKBESelectionFlags                                      UIWKSelectionFlags
#define WKBESelectionFlagsNone                                  UIWKNone
#define WKBEWordIsNearTap                                       UIWKWordIsNearTap
#define WKBESelectionFlipped                                    UIWKSelectionFlipped
#define WKBEPhraseBoundaryChanged                               UIWKPhraseBoundaryChanged
#endif

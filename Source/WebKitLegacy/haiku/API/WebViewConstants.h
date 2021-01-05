/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 *
 * All rights reserved.
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

#ifndef WebViewConstants_h
#define WebViewConstants_h

enum {
    LOAD_ONLOAD_HANDLE =                                300,
    LOAD_NEGOTIATING =                                  301,
    TITLE_CHANGED =                                     302,
    LOAD_COMMITTED =                                    303,
    LOAD_PROGRESS =                                     304,
    LOAD_DOC_COMPLETED =                                305,
    LOAD_DL_COMPLETED =                                 306,
    LOAD_FAILED =                                       307,
    LOAD_FINISHED =                                     308,
    NEW_WINDOW_REQUESTED =                              309,
    NEW_PAGE_CREATED =                                  310,
    NAVIGATION_REQUESTED =                              311,
    JAVASCRIPT_WINDOW_OBJECT_CLEARED =                  312,
    UPDATE_HISTORY =                                    313,
    UPDATE_NAVIGATION_INTERFACE =                       314,
    AUTHENTICATION_CHALLENGE =                          315,
    ICON_RECEIVED =                                     316,
    CLOSE_WINDOW_REQUESTED =                            317,
    MAIN_DOCUMENT_ERROR =                               318,
    LOAD_STARTED =                                      319,
    RESPONSE_RECEIVED =                                 320,
    ICON_CHANGED =                                      321,
    SSL_CERT_ERROR =                                    322
};

enum {
    WebKitErrorCannotShowMIMEType =                     100,
    WebKitErrorCannotShowURL =                          101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =     102,
    WebKitErrorCannotUseRestrictedPort =                103,
    WebKitErrorCannotFindPlugIn =                       200,
    WebKitErrorPlugInWillHandleLoad =                   201,
    WebKitErrorJavaUnavailable =                        202
};

enum {
    TOOLBARS_VISIBILITY =                               401,
    STATUSBAR_VISIBILITY =                              402,
    MENUBAR_VISIBILITY =                                403,
    SET_RESIZABLE =                                     405,
    SET_STATUS_TEXT =                                   406,
    RESIZING_REQUESTED =                                407,
    ADD_CONSOLE_MESSAGE =                               408,
    SHOW_JS_ALERT =                                     409,
    SHOW_JS_CONFIRM =                                   410,
};

enum {
    EDITOR_DELETE_RANGE =                               500,
    EDITOR_BEGIN_EDITING =                              501,
    EDITOR_EDITING_BEGAN =                              502,
    EDITOR_EDITING_ENDED =                              503,
    EDITOR_END_EDITING =                                504,
    EDITOR_INSERT_NODE =                                505,
    EDITOR_INSERT_TEXT =                                506,
    EDITOR_CHANGE_SELECTED_RANGE =                      507,
    EDITOR_APPLY_STYLE =                                508,
    EDITOR_SELECTION_CHANGED =                          509,
    EDITOR_CONTENTS_CHANGED =                           510
};

#endif // WebViewConstants_h

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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(WIN)

#define RESET
#define ANSI_COLOR(COLOR)

#else

#define CSI "\033["
#define RESET , CSI "0m"
#define ANSI_COLOR(COLOR) CSI COLOR,

#endif

#define BLACK(CONTENT) ANSI_COLOR("30m") CONTENT RESET
#define RED(CONTENT) ANSI_COLOR("31m") CONTENT RESET
#define GREEN(CONTENT) ANSI_COLOR("32m") CONTENT RESET
#define YELLOW(CONTENT) ANSI_COLOR("33m") CONTENT RESET
#define BLUE(CONTENT) ANSI_COLOR("34m") CONTENT RESET
#define MAGENTA(CONTENT) ANSI_COLOR("35m") CONTENT RESET
#define CYAN(CONTENT) ANSI_COLOR("36m") CONTENT RESET
#define WHITE(CONTENT) ANSI_COLOR("37m") CONTENT RESET

#define BOLD(CONTENT) ANSI_COLOR("1m") CONTENT RESET


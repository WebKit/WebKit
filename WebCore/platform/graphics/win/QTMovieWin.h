/*
 * Copyright (C) 2007, 2008, 2009 Apple, Inc.  All rights reserved.
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

#ifndef QTMovieWin_h
#define QTMovieWin_h

#include <Unicode.h>

#ifdef QTMOVIEWIN_EXPORTS
#define QTMOVIEWIN_API __declspec(dllexport)
#else
#define QTMOVIEWIN_API __declspec(dllimport)
#endif

class QTMovieWin;
class QTMovieWinPrivate;

class QTMovieWinClient {
public:
    virtual void movieEnded(QTMovieWin*) = 0;
    virtual void movieLoadStateChanged(QTMovieWin*) = 0;
    virtual void movieTimeChanged(QTMovieWin*) = 0;
    virtual void movieNewImageAvailable(QTMovieWin*) = 0;
};

enum {
    QTMovieLoadStateError = -1L,
    QTMovieLoadStateLoaded  = 2000L,
    QTMovieLoadStatePlayable = 10000L,
    QTMovieLoadStatePlaythroughOK = 20000L,
    QTMovieLoadStateComplete = 100000L
};

typedef const struct __CFURL * CFURLRef;

class QTMOVIEWIN_API QTMovieWin {
public:
    static bool initializeQuickTime();

    QTMovieWin(QTMovieWinClient*);
    ~QTMovieWin();

    void load(const UChar* url, int len, bool preservesPitch);
    long loadState() const;
    float maxTimeLoaded() const;

    void play();
    void pause();

    float rate() const;
    void setRate(float);

    float duration() const;
    float currentTime() const;
    void setCurrentTime(float) const;

    void setVolume(float);
    void setPreservesPitch(bool);

    unsigned dataSize() const;

    void getNaturalSize(int& width, int& height);
    void setSize(int width, int height);

    void setVisible(bool);
    void paint(HDC, int x, int y);

    void disableUnsupportedTracks(unsigned& enabledTrackCount, unsigned& totalTrackCount);
    void setDisabled(bool);

    bool hasVideo() const;

    static unsigned countSupportedTypes();
    static void getSupportedType(unsigned index, const UChar*& str, unsigned& len);

private:
    void load(CFURLRef, bool preservesPitch);

    QTMovieWinPrivate* m_private;
    bool m_disabled;
    friend class QTMovieWinPrivate;
};

#endif

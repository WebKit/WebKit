/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQDateTime.h"

#import <time.h>

static CFTimeZoneRef systemTimeZone()
{
    static CFTimeZoneRef zone = CFTimeZoneCopySystem();
    return zone;
}

QTime::QTime(int hours, int minutes)
{
    CFGregorianDate date;
    date.year = 2001;
    date.month = 1;
    date.day = 1;
    date.hour = hours;
    date.minute = minutes;
    date.second = 0;
    timeInSeconds = CFGregorianDateGetAbsoluteTime(date, systemTimeZone());
}

int QTime::msec() const
{
    return (int)(timeInSeconds * 1000) % 1000;
}

QTime QTime::addMSecs(int msecs) const
{
    QTime newTime(*this);
    newTime.timeInSeconds += msecs * 0.001;
    return newTime;
}

int QTime::elapsed() const
{
    CFTimeInterval elapsed = CFAbsoluteTimeGetCurrent() - timeInSeconds;
    return (int)(elapsed * 1000);
}

int QTime::restart()
{
    CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
    CFTimeInterval elapsed = currentTime - timeInSeconds;    
    timeInSeconds = currentTime;
    return (int)(elapsed * 1000);
}

QDate::QDate(int y, int m, int d)
    : year(y), month(m), day(d)
{
}

QDateTime::QDateTime(const QDate &d, const QTime &t)
{
    CFGregorianDate dateWithTime = CFAbsoluteTimeGetGregorianDate(t.timeInSeconds, systemTimeZone());
    dateWithTime.year = d.year;
    dateWithTime.month = d.month;
    dateWithTime.day = d.day;
    dateInSeconds = CFGregorianDateGetAbsoluteTime(dateWithTime, systemTimeZone());
}

int QDateTime::secsTo(const QDateTime &b) const
{
    return (int)(b.dateInSeconds - dateInSeconds);
}

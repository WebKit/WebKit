/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#include <kwqdebug.h>

#include <qapplication.h>
#include <qpalette.h>

QPalette QApplication::palette(const QWidget *p)
{
    static QPalette pal;
    return pal;
}

// Do we need to worry about multiple screens?

int QDesktopWidget::width() const
{
    return (int)[[NSScreen mainScreen] frame].size.width;
}
    
int QDesktopWidget::height() const
{
    return (int)[[NSScreen mainScreen] frame].size.height;
}

int QDesktopWidget::screenNumber(QWidget *) const
{
    return 0;
}

QRect QDesktopWidget::screenGeometry(int screenNumber)
{
    NSRect rect = [[NSScreen mainScreen] frame];
    return QRect((int)rect.origin.x, (int)rect.origin.y, (int)rect.size.width, (int)rect.size.height);
}

QDesktopWidget *QApplication::desktop()
{
    static QDesktopWidget *desktopWidget;
    if (desktopWidget == 0) {
        desktopWidget = new QDesktopWidget();
    }
    return desktopWidget;
}


int QApplication::startDragDistance()
{
    _logNotYetImplemented();
     return 2;
}


QSize QApplication::globalStrut()
{
    _logNotYetImplemented();
    return QSize(0,0);
}


void QApplication::setOverrideCursor(const QCursor &c)
{
    _logNotYetImplemented();
}


void QApplication::restoreOverrideCursor()
{
    _logNotYetImplemented();
}


bool QApplication::sendEvent(QObject *o, QEvent *e)
{
    KWQDEBUG ("received %d\n", e->type());
    return FALSE;
}

void QApplication::sendPostedEvents(QObject *receiver, int event_type)
{
    _logNotYetImplemented();
}


QApplication::QApplication( int &argc, char **argv)
{
#ifdef TRANSITIONAL_CODE
    _initialize();
#endif    
}

QApplication *qApp = NULL;

#ifdef TRANSITIONAL_CODE
void QApplication::_initialize()
{
    NSDictionary *info;
    NSString *principalClassName;
    NSString *mainNibFile;

    globalPool = [[NSAutoreleasePool allocWithZone:NULL] init];
    info = [[NSBundle mainBundle] infoDictionary];
    principalClassName = [info objectForKey:@"NSPrincipalClass"];
    mainNibFile = [info objectForKey:@"NSMainNibFile"];

    if (principalClassName) {
        Class principalClass = NSClassFromString(principalClassName);
	if (principalClass) {
            application = [principalClass sharedApplication];
	    if (![NSBundle loadNibNamed: mainNibFile owner: application]) {
                KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR:  QApplication::_initialize() unable to load %s\n", DEBUG_OBJECT(mainNibFile));
            }
        }
    }	
    
    //  Force linkage
    [_KWQOwner class];

    if (qApp == NULL) {
	qApp = this;
    }
}
#endif


QApplication::~QApplication()
{
    [globalPool release];
}


void QApplication::setMainWidget(QWidget *w)
{
#ifdef TRANSITIONAL_CODE
    NSRect b = [((_KWQOwner *)application)->containerView bounds];
    
    if (application == nil){
        KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR: QApplication::setMainWidget() application not set.\n");
        return;
    }
    if (w == 0){
        KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR: QApplication::setMainWidget() widget not valid.\n");
        return;
    }
    
    mainWidget = w;
    
    NSScrollView *sv = [[NSScrollView alloc] initWithFrame: NSMakeRect (0,0,b.size.width,b.size.height)];
    [sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setHasVerticalScroller: YES];
    [sv setHasHorizontalScroller: YES];
    [w->getView() setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: w->getView()];
    [((_KWQOwner *)application)->window setOpaque: FALSE];
    //[((_KWQOwner *)application)->window setAlphaValue: (float)0.8];
    
     
    [((_KWQOwner *)application)->containerView addSubview: sv];
#else
    [NSException raise:@"Not implemented" format:@"QApplication::setMainWidget(QWidget *) is not implemented"];
#endif
}


int QApplication::exec()
{
#ifdef TRANSITIONAL_CODE
    if (application == nil){
        KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR: QApplication::exec() application not set.\n");
        return 0;
    }
    [application run];
    return 1;
#else
    [NSException raise:@"Not implemented" format:@"QApplication::exec() is not implemented"];
    return 0;
#endif
}


QWidget *QApplication::focusWidget() const
{
    _logNeverImplemented();
    return NULL;
}

QStyle &QApplication::style() const
{
    static QStyle style;
    return style;
}

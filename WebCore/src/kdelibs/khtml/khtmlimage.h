/* This file is part of the KDE project
   Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __khtmlimage_h__
#define __khtmlimage_h__

#include <kparts/part.h>
#include <kparts/factory.h>
#include <kparts/browserextension.h>

class KHTMLPart;
class KInstance;

class KHTMLImageFactory : public KParts::Factory
{
    Q_OBJECT
public:
    KHTMLImageFactory();
    virtual ~KHTMLImageFactory();

    virtual KParts::Part *createPartObject( QWidget *parentWidget, const char *widgetName,
                                            QObject *parent, const char *name,
                                            const char *className, const QStringList &args );

    static KInstance *instance() { return s_instance; }

private:
    static KInstance *s_instance;
};

class KHTMLImage : public KParts::ReadOnlyPart
{
    Q_OBJECT
public:
    KHTMLImage( QWidget *parentWidget, const char *widgetName,
                QObject *parent, const char *name );
    virtual ~KHTMLImage();

    virtual bool openFile() { return true; } // grmbl, should be non-pure in part.h, IMHO

    virtual bool openURL( const KURL &url );

    virtual bool closeURL();

    KHTMLPart *doc() const { return m_khtml; }

protected:
    virtual void guiActivateEvent( KParts::GUIActivateEvent *e );

private slots:
    void slotPopupMenu( KXMLGUIClient *cl, const QPoint &pos, const KURL &u, const QString &mime, mode_t mode );
    void slotImageJobFinished( KIO::Job *job );

    void updateWindowCaption();

private:
    QGuardedPtr<KHTMLPart> m_khtml;
    KParts::BrowserExtension *m_ext;
    QString m_mimeType;
};

class KHTMLImageBrowserExtension : public KParts::BrowserExtension
{
    Q_OBJECT
public:
    KHTMLImageBrowserExtension( KHTMLImage *parent, const char *name = 0 );

    virtual int xOffset();
    virtual int yOffset();

protected slots:
    void print();
    void reparseConfiguration();

private:
    KHTMLImage *m_imgPart;
};

#endif

/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __khtml_run_h__
#define __khtml_run_h__

#include <krun.h>
#include <kurl.h>
#include <kservice.h>
#include <kparts/browserextension.h>

class KHTMLPart;

namespace khtml
{
  struct ChildFrame;
};

class KHTMLRun : public KRun
{
  Q_OBJECT
public:
  KHTMLRun( KHTMLPart *part, khtml::ChildFrame *child, const KURL &url, 
            const KParts::URLArgs &args, bool hideErrorDialog );

  virtual void foundMimeType( const QString &mimetype );

  KHTMLPart *part() const { return m_part; }
  KParts::URLArgs urlArgs() const { return m_args; }

protected:
  virtual void scanFile();

  bool allowExecution( const QString &serviceType, const KURL &url );
  bool isExecutable( const QString &serviceType );
  bool askSave( const KURL & url, KService::Ptr offer, const QString & mimeType, const QString & suggestedFilename );

protected slots:
  void slotKHTMLScanFinished(KIO::Job *job);
  void slotKHTMLMimetype(KIO::Job *job, const QString &type);
  void slotStatResult( KIO::Job *job );

private:
  void handleError();

  KHTMLPart *m_part;
  KParts::URLArgs m_args;
  khtml::ChildFrame *m_child;
  QString m_suggestedFilename;
  bool m_hideErrorDialog;
};

#endif

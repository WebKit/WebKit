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
#include "khtml_run.h"
#include "khtml_part.h"
#include <kio/job.h>
#include <kdebug.h>
#include <kuserprofile.h>
#include <kmessagebox.h>
#include <kstringhandler.h>
#include <klocale.h>
#include <khtml_ext.h>

KHTMLRun::KHTMLRun( KHTMLPart *part, khtml::ChildFrame *child, const KURL &url,
                    const KParts::URLArgs &args, bool hideErrorDialog )
: KRun( url, 0, false, false /* No GUI */ ) , m_part( part ),
  m_args( args ), m_child( child ), m_hideErrorDialog( hideErrorDialog )
{
}

void KHTMLRun::foundMimeType( const QString &_type )
{
    QString mimeType = _type; // this ref comes from the job, we lose it when using KIO again
    if ( !m_part->processObjectRequest( m_child, m_strURL, mimeType ) )
    {
       if ( !m_bFinished && // couldn't embed
            mimeType != "inode/directory" && // dirs can't be saved
            !m_strURL.isLocalFile() ) // ... and remote URL
       {
           KService::Ptr offer = KServiceTypeProfile::preferredService(mimeType, true);
           if ( askSave( m_strURL, offer, mimeType, m_suggestedFilename ) ) // ... -> ask whether to save
           { // true: saving done or canceled
               m_bFinished = true;
               m_bFault = true; // make Konqueror think there was an error, in order to stop the spinning wheel
           }
       }

       // Check if running is allowed
       if ( !m_bFinished &&  //     If not embedddable ...
            !allowExecution( mimeType, m_strURL ) ) // ...and the user said no (for executables etc.)
       {
           m_bFinished = true;
           //m_bFault = true; // might not be necessary in khtml
       }

       if ( m_bFinished )
       {
           m_timer.start( 0, true );
           return;
       }

       kdDebug(6050) << "KHTMLRun::foundMimeType " << _type << " couldn't open" << endl;
       KRun::foundMimeType( mimeType );
       return;
    }
    m_bFinished = true;
    m_timer.start( 0, true );
}

bool KHTMLRun::allowExecution( const QString &serviceType, const KURL &url )
{
    if ( !isExecutable( serviceType ) )
      return true;

    return ( KMessageBox::warningYesNo( 0, i18n( "Do you really want to execute '%1' ? " ).arg( url.prettyURL() ) ) == KMessageBox::Yes );
}

bool KHTMLRun::isExecutable( const QString &serviceType )
{
    return ( serviceType == "application/x-desktop" ||
             serviceType == "application/x-executable" ||
             serviceType == "application/x-shellscript" );
}

bool KHTMLRun::askSave( const KURL & url, KService::Ptr offer, const QString & mimeType, const QString & suggestedFilename )
{
    QString surl = KStringHandler::csqueeze( url.prettyURL() );
    // Inspired from kmail
    QString question = offer ? i18n("Open '%1' using '%2'?").
                               arg( surl ).arg(offer->name())
                       : i18n("Open '%1' ?").arg( surl );
    int choice = KMessageBox::warningYesNoCancel(
        0L, question, QString::null,
        i18n("Save to disk"), i18n("Open"),
        QString::fromLatin1("askSave")+mimeType); // dontAskAgainName
    if ( choice == KMessageBox::Yes ) // Save
        KHTMLPopupGUIClient::saveURL( m_part->widget(), i18n( "Save As..." ), url, QString::null, 0, suggestedFilename );

    return choice != KMessageBox::No; // saved or canceled -> don't open
}

void KHTMLRun::scanFile()
{
  if (m_strURL.protocol().left(4) != "http") // http and https
  {
     KRun::scanFile();
     return;
  }

  // No check for well-known extensions, since we don't trust HTTP

  KIO::TransferJob *job;
  if ( m_args.doPost() )
  {
      job = KIO::http_post( m_strURL, m_args.postData, false );
      job->addMetaData("content-type", m_args.contentType());
  }
  else
      job = KIO::get(m_strURL, false, false);

  job->addMetaData(m_args.metaData());

  //job->setWindow((KMainWindow *)m_pMainWindow);
  connect( job, SIGNAL( result( KIO::Job *)),
           this, SLOT( slotKHTMLScanFinished(KIO::Job *)));
  connect( job, SIGNAL( mimetype( KIO::Job *, const QString &)),
           this, SLOT( slotKHTMLMimetype(KIO::Job *, const QString &)));
  m_job = job;
}

void KHTMLRun::slotKHTMLScanFinished(KIO::Job *job)
{
  if ( m_hideErrorDialog && job->error() )
      handleError();
  else
      KRun::slotScanFinished(job);
}

void KHTMLRun::slotKHTMLMimetype(KIO::Job *, const QString &type)
{
  KIO::TransferJob *job = (KIO::TransferJob *) m_job;
  // Update our URL in case of a redirection
  m_strURL = job->url();

  m_suggestedFilename = job->queryMetaData("content-disposition");

  // Make copy to avoid dead reference
  QString _type = type;
  job->putOnHold();
  m_job = 0;

  foundMimeType( _type );
}

void KHTMLRun::slotStatResult( KIO::Job *job )
{
    if ( m_hideErrorDialog && job->error() )
        handleError();
    else
        KRun::slotStatResult( job );
}

void KHTMLRun::handleError()
{
    // pass an empty url and mimetype to indicate a loading error
    m_part->processObjectRequest( m_child, KURL(), QString::null );
    m_job = 0;
    m_bFault = true;
    m_bFinished = true;

    m_timer.start( 0, true );
}

#include "khtml_run.moc"

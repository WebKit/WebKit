/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef khtmlpart_p_h
#define khtmlpart_p_h

#include <kcursor.h>
#include <klibloader.h>
#include <kxmlguifactory.h>
#include <kaction.h>
#include <kparts/partmanager.h>

#include "khtml_run.h"
#include "khtml_find.h"
#include "khtml_factory.h"
#include "khtml_events.h"
#include "khtml_ext.h"
#include "khtml_iface.h"
#include "khtml_settings.h"
#include "misc/decoder.h"
#include "misc/formdata.h"
#include "java/kjavaappletcontext.h"
#include "ecma/kjs_proxy.h"
#include "css/css_valueimpl.h"
#include "editing/edit_command.h"
#include "editing/SelectionController.h"

namespace KIO
{
  class Job;
  class TransferJob;
}

namespace khtml
{
  struct ChildFrame
  {
      enum Type { Frame, IFrame, Object };

      ChildFrame() { m_bCompleted = false; m_bPreloaded = false; m_type = Frame; m_bNotify = false; m_hasFallbackContent = false; }


    QGuardedPtr<khtml::RenderPart> m_frame;
    QGuardedPtr<KParts::ReadOnlyPart> m_part;
    QGuardedPtr<KParts::BrowserExtension> m_extension;
    QString m_serviceName;
    QString m_serviceType;
    QStringList m_services;
    bool m_bCompleted;
    QString m_name;
    KParts::URLArgs m_args;
    bool m_bPreloaded;
    KURL m_workingURL;
    Type m_type;
    QStringList m_paramNames;
    QStringList m_paramValues;
    bool m_bNotify;
    bool m_hasFallbackContent;
  };
}

class FrameList : public QValueList<khtml::ChildFrame>
{
public:
    Iterator find( const QString &name );
};

typedef FrameList::ConstIterator ConstFrameIt;
typedef FrameList::Iterator FrameIt;

static int khtml_part_dcop_counter = 0;

enum RedirectionScheduled {
    noRedirectionScheduled,
    redirectionScheduled,
    locationChangeScheduled,
    historyNavigationScheduled,
    locationChangeScheduledDuringLoad
};

class KHTMLPartPrivate
{
public:
  KHTMLPartPrivate(QObject* parent)
  {
    m_doc = 0L;
    m_decoder = 0L;
    m_jscript = 0L;
    m_runningScripts = 0;
    m_kjs_lib = 0;
    m_job = 0L;
    m_bComplete = true;
    m_bLoadingMainResource = false;
    m_bLoadEventEmitted = true;
    m_bUnloadEventEmitted = true;
    m_cachePolicy = KIO::CC_Verify;
    m_bClearing = false;
    m_bCleared = true;
    m_zoomFactor = 100;
    m_bDnd = true;
    m_haveEncoding = false;
    m_activeFrame = 0L;
#ifndef Q_WS_QWS
    m_javaContext = 0;
#endif
    m_frameNameId = 1;

    m_restored = false;

    m_focusNodeNumber = -1;
    m_focusNodeRestored = false;

    m_bJScriptForce = false;
    m_bJScriptOverride = false;
    m_bJavaForce = false;
    m_bJavaOverride = false;
    m_bPluginsForce = false;
    m_bPluginsOverride = false;
    m_onlyLocalReferences = false;

    m_caretBlinkTimer = 0;
    m_caretVisible = false;
    m_caretBlinks = true;
    m_caretPaint = true;
    
    m_typingStyle = 0;

    m_metaRefreshEnabled = true;
    m_bHTTPRefresh = false;

    m_bFirstData = true;
    m_submitForm = 0;
    m_scheduledRedirection = noRedirectionScheduled;
    m_delayRedirect = 0;

    m_bPendingChildRedirection = false;
    m_executingJavaScriptFormAction = false;

    m_cancelWithLoadInProgress = false;
    
    // inherit settings from parent
    if(parent && parent->inherits("KHTMLPart"))
    {
        KHTMLPart* part = static_cast<KHTMLPart*>(parent);
        if(part->d)
        {
            m_bJScriptForce = part->d->m_bJScriptForce;
            m_bJScriptOverride = part->d->m_bJScriptOverride;
            m_bJavaForce = part->d->m_bJavaForce;
            m_bJavaOverride = part->d->m_bJavaOverride;
            m_bPluginsForce = part->d->m_bPluginsForce;
            m_bPluginsOverride = part->d->m_bPluginsOverride;
            // Same for SSL settings
            m_onlyLocalReferences = part->d->m_onlyLocalReferences;
            m_zoomFactor = part->d->m_zoomFactor;
        }
    }

    m_isFocused = false;
    m_focusNodeNumber = -1;
    m_focusNodeRestored = false;
    m_opener = 0;
    m_openedByJS = false;
    m_newJSInterpreterExists = false;
    m_dcopobject = 0;
    m_dcop_counter = ++khtml_part_dcop_counter;
  }
  ~KHTMLPartPrivate()
  {
    delete m_dcopobject;
    delete m_extension;
    delete m_jscript;
    if ( m_kjs_lib)
       m_kjs_lib->unload();
#ifndef Q_WS_QWS
    delete m_javaContext;
#endif
    if (m_typingStyle)
        m_typingStyle->deref();
  }

  FrameList m_frames;
  QValueList<khtml::ChildFrame> m_objects;

  QGuardedPtr<KHTMLView> m_view;
  KHTMLPartBrowserExtension *m_extension;
  KHTMLPartBrowserHostExtension *m_hostExtension;
  DOM::DocumentImpl *m_doc;
  khtml::Decoder *m_decoder;
  QString m_encoding;
  QString m_sheetUsed;
#if !APPLE_CANGES
  long m_cacheId;
#endif
  QString scheduledScript;
  SharedPtr<DOM::NodeImpl> scheduledScriptNode;

  KJSProxy *m_jscript;
  KLibrary *m_kjs_lib;
  int m_runningScripts;
  bool m_bJScriptEnabled :1;
  bool m_bJScriptDebugEnabled :1;
  bool m_bJavaEnabled :1;
  bool m_bPluginsEnabled :1;
  bool m_bJScriptForce :1;
  bool m_bJScriptOverride :1;
  bool m_bJavaForce :1;
  bool m_bJavaOverride :1;
  bool m_bPluginsForce :1;
  bool m_metaRefreshEnabled :1;
  bool m_bPluginsOverride :1;
  bool m_restored :1;
  int m_frameNameId;
  int m_dcop_counter;
  DCOPObject *m_dcopobject;

#ifndef Q_WS_QWS
  KJavaAppletContext *m_javaContext;
#endif

  KHTMLSettings *m_settings;

  KIO::TransferJob * m_job;

  QString m_kjsStatusBarText;
  QString m_kjsDefaultStatusBarText;
  QString m_lastModified;


  bool m_bComplete:1;
  bool m_bLoadingMainResource:1;
  bool m_bLoadEventEmitted:1;
  bool m_bUnloadEventEmitted:1;
  bool m_haveEncoding:1;
  bool m_bHTTPRefresh:1;
  bool m_onlyLocalReferences :1;
  bool m_redirectLockHistory:1;
  bool m_redirectUserGesture:1;
  
  KURL m_workingURL;

  KIO::CacheControl m_cachePolicy;
  QTimer m_redirectionTimer;
  QTime m_parsetime;

  RedirectionScheduled m_scheduledRedirection;
  double m_delayRedirect;
  QString m_redirectURL;
  QString m_redirectReferrer;
  int m_scheduledHistoryNavigationSteps;


  int m_zoomFactor;


  QString m_strSelectedURL;
  QString m_strSelectedURLTarget;
  QString m_referrer;

  struct SubmitForm
  {
    const char *submitAction;
    QString submitUrl;
    khtml::FormData submitFormData;
    QString target;
    QString submitContentType;
    QString submitBoundary;
  };

  SubmitForm *m_submitForm;

  bool m_bMousePressed;
  SharedPtr<DOM::NodeImpl> m_mousePressNode; //node under the mouse when the mouse was pressed (set in the mouse handler)

  khtml::ETextGranularity m_selectionGranularity;
  bool m_beganSelectingText;

  khtml::SelectionController m_selection;
  khtml::SelectionController m_dragCaret;
  khtml::SelectionController m_mark;
  int m_caretBlinkTimer;

  bool m_caretVisible:1;
  bool m_caretBlinks:1;
  bool m_caretPaint:1;
  bool m_bDnd:1;
  bool m_bFirstData:1;
  bool m_bClearing:1;
  bool m_bCleared:1;
  bool m_bSecurityInQuestion:1;
  bool m_focusNodeRestored:1;
  bool m_isFocused:1;

  khtml::EditCommandPtr m_lastEditCommand;
  int m_xPosForVerticalArrowNavigation;
  DOM::CSSMutableStyleDeclarationImpl *m_typingStyle;

  int m_focusNodeNumber;

  QPoint m_dragStartPos;
#ifdef KHTML_NO_SELECTION
  QPoint m_dragLastPos;
#endif


  //QGuardedPtr<KParts::Part> m_activeFrame;
  KParts::Part * m_activeFrame;
  QGuardedPtr<KHTMLPart> m_opener;
  bool m_openedByJS;
  bool m_newJSInterpreterExists; // set to 1 by setOpenedByJS, for window.open

  bool m_bPendingChildRedirection;

  bool m_executingJavaScriptFormAction;
  
  bool m_cancelWithLoadInProgress;

  QTimer m_lifeSupportTimer;
  
};

#endif

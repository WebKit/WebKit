/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000-2001 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef KJS_DEBUGGER

#include "kjs_debugwin.h"

#include <qlayout.h>
#include <qpushbutton.h>
#include <qmultilineedit.h>
#include <qapplication.h>
#include <klocale.h>
#include <kdebug.h>

using namespace KJS;

KJSDebugWin::KJSDebugWin(QWidget *parent, const char *name)
  : QWidget(parent, name),
    Debugger(0L),
    modal(false),
    currentSrcId(-1)
{
  srcDict.setAutoDelete(true);
  setCaption(i18n("JavaScript Debugger"));
  QVBoxLayout *vl = new QVBoxLayout(this, 5);
  edit = new QMultiLineEdit(this);
  edit->setMinimumSize(600, 100);
  edit->setReadOnly(true);
  vl->addWidget(edit);

  QHBoxLayout *hl = new QHBoxLayout(vl);
  nextButton = new QPushButton(i18n("&Next"), this);
  stepButton = new QPushButton(i18n("&Step"), this);
  continueButton = new QPushButton(i18n("&Continue"), this);
  hl->addWidget(nextButton);
  hl->addWidget(stepButton);
  hl->addWidget(continueButton);
  hl->addStretch();

  connect(nextButton, SIGNAL(clicked()), SLOT(next()));
  connect(stepButton, SIGNAL(clicked()), SLOT(step()));
  connect(continueButton, SIGNAL(clicked()), SLOT(cont()));

  nextButton->setEnabled(false);
  stepButton->setEnabled(false);
  continueButton->setEnabled(false);
}

KJSDebugWin::~KJSDebugWin()
{
  detach();
}

void KJSDebugWin::next()
{
  setMode(Debugger::Next);
  leaveSession();
}

void KJSDebugWin::step()
{
  setMode(Debugger::Step);
  leaveSession();
}

void KJSDebugWin::cont()
{
  setMode(Debugger::Continue);
  leaveSession();
}

bool KJSDebugWin::stopEvent()
{
  if (!isVisible())
    show();
  if ( sourceId() != currentSrcId ) {
      currentSrcId = sourceId();
      QString *s = srcDict[ currentSrcId ];
      if ( s ) {
	edit->setText(*s);
      } else {
	kdWarning() << "KJSDebugWin: unknown source id." << endl;
	edit->setText("????????");
      }
  }
  highLight(lineNumber());
  enterSession();
  return true;
}

void KJSDebugWin::setCode(const QString &code)
{
  edit->setText(code);
  if ( !code.isNull() ) {
      srcDict.insert(freeSourceId(), new QString(code));
  }
}

void KJSDebugWin::highLight(int line)
{
  if (line >= 0) {
    edit->setCursorPosition(line, 0, false);
    edit->setCursorPosition(line, 99, true);
  } else {
    edit->setCursorPosition(0, 0, false);
    edit->clearFocus();
  }
}

void KJSDebugWin::enterSession()
{
  assert(!modal);
  modal = true;
  nextButton->setEnabled(true);
  stepButton->setEnabled(true);
  continueButton->setEnabled(true);
  qApp->enter_loop();
  assert(!modal);
}

void KJSDebugWin::leaveSession()
{
  assert(modal);
  nextButton->setEnabled(false);
  stepButton->setEnabled(false);
  continueButton->setEnabled(false);
  modal = false;
  qApp->exit_loop();
}

#include "kjs_debugwin.moc"

#endif

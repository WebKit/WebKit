/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include "kjs_debugwin.h"

#ifdef KJS_DEBUGGER

#include <qlayout.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qlistbox.h>
#include <qlineedit.h>
#include <qapplication.h>
#include <qsplitter.h>
#include <qcombobox.h>
#include <qbitmap.h>
#include <qwidgetlist.h>

#include <klocale.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <kmessagebox.h>

#include "kjs_dom.h"

using namespace KJS;

static KJSDebugWin *kjs_html_debugger = 0;


bool FakeModal::eventFilter( QObject *o, QEvent *e )
{
    switch (e->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::Destroy:
        case QEvent::Close:
        case QEvent::Quit:
            while (o->parent())
                o = o->parent();
            if (o == modalWidget)
                return QWidget::eventFilter( o, e );
            else
                return TRUE;
            break;
        default:
            return QWidget::eventFilter( o, e );
    }
}


void FakeModal::enable(QWidget *modal)
{
    QWidgetList *widgets = QApplication::allWidgets();
    QWidgetListIt it(*widgets);
    for (; it.current(); ++it)
        it.current()->installEventFilter(this);
    modalWidget = modal;
}

void FakeModal::disable()
{
    QWidgetList *widgets = QApplication::allWidgets();
    QWidgetListIt it(*widgets);
    for (; it.current(); ++it)
        it.current()->removeEventFilter(this);
    modalWidget = 0;
}

//-------------------------------------------------------------------------

QString StackFrame::toString()
{
  QString str = "";
  if (!name.isNull())
    str.sprintf("%s() at sourceId %d, line %d",name.ascii(),sourceId,lineno);
  else
    str.sprintf("??? at sourceId %d, line %d",sourceId,lineno);
  return str;
}

SourceFragment::SourceFragment(int sid, int bl, SourceFile *sf)
{
  sourceId = sid;
  baseLine = bl;
  sourceFile = sf;
  sourceFile->ref();
}

SourceFragment::~SourceFragment()
{
  sourceFile->deref();
}

//-------------------------------------------------------------------------

KJSDebugWin::KJSDebugWin(QWidget *parent, const char *name)
  : QWidget(parent, name),
    m_inSession(false),
    m_curSourceFile(0)
{
  setCaption(i18n("JavaScript Debugger"));
  QVBoxLayout *vl = new QVBoxLayout(this, 5);

  // frame list & code
  QSplitter *splitter = new QSplitter(this);
  QFont font("courier",10);

  m_frameList = new QListBox(splitter);
  m_frameList->setFont(font);
  m_frameList->setMinimumSize(100,200);
  connect(m_frameList,SIGNAL(highlighted(int)),this,SLOT(showFrame(int)));

  // source selection & display
  QWidget *sourceSelDisplay = new QWidget(splitter);
  QVBoxLayout *ssdvl = new QVBoxLayout(sourceSelDisplay);
  
  
  m_sourceSel = new QComboBox(sourceSelDisplay);
  connect(m_sourceSel,SIGNAL(activated(int)),this,SLOT(sourceSelected(int)));
  ssdvl->addWidget(m_sourceSel);

  m_sourceDisplay = new QListBox(sourceSelDisplay);
  m_sourceDisplay->setFont(font);
  ssdvl->addWidget(m_sourceDisplay);

  vl->addWidget(splitter);

  QValueList<int> splitSizes;
  splitSizes.insert(splitSizes.end(),200);
  splitSizes.insert(splitSizes.end(),400);
  splitter->setSizes(splitSizes);


  // evaluate
  QHBoxLayout *hl1 = new QHBoxLayout(vl);
  m_evalEdit = new QLineEdit(this);
  m_evalButton = new QPushButton(i18n("&Evaluate"),this);
  m_evalButton->setEnabled(false);
  hl1->addWidget(m_evalEdit);
  hl1->addWidget(m_evalButton);
  connect(m_evalButton, SIGNAL(clicked()), SLOT(eval()));
  connect(m_evalEdit, SIGNAL(returnPressed()), SLOT(eval()));

  // control buttons
  QHBoxLayout *hl2 = new QHBoxLayout(vl);
  m_nextButton = new QPushButton(i18n("&Next"), this);
  m_stepButton = new QPushButton(i18n("&Step"), this);
  m_continueButton = new QPushButton(i18n("&Continue"), this);
  m_stopButton = new QPushButton(i18n("St&op"), this);
  hl2->addWidget(m_nextButton);
  hl2->addWidget(m_stepButton);
  hl2->addWidget(m_continueButton);
  hl2->addWidget(m_stopButton);
  hl2->addStretch();

  connect(m_nextButton, SIGNAL(clicked()), SLOT(next()));
  connect(m_stepButton, SIGNAL(clicked()), SLOT(step()));
  connect(m_continueButton, SIGNAL(clicked()), SLOT(cont()));
  connect(m_stopButton, SIGNAL(clicked()), SLOT(stop()));

  m_nextButton->setEnabled(false);
  m_stepButton->setEnabled(false);
  m_continueButton->setEnabled(false);
  m_stopButton->setEnabled(false);

  // frame list
  m_frames.setAutoDelete(true);
  StackFrame *sf = new StackFrame(-1,-1,"Global code",false);
  sf->next = true;
  m_frames.append(sf);
  //  m_frameList->insertItem(sf->toString());


  setMinimumSize(300,200);
  resize(600,450);
  m_mode = Continue;
  m_sourceBreakpoints = 0;

  KIconLoader loader;
  m_stopIcon = loader.loadIcon("stop",KIcon::Small);
  
  m_emptyIcon = QPixmap(m_stopIcon.width(),m_stopIcon.height());
  QBitmap emptyMask(m_stopIcon.width(),m_stopIcon.height(),true);
  //  m_emptyIcon.fill(m_sourceDisplay,0,0);
  m_emptyIcon.setMask(emptyMask);

  m_nextSourceBaseLine = 0;
  m_nextSourceUrl = "";

  updateFrameList();
  m_inSession = false;
  m_curContext = 0;
  m_curScript = 0;
}

KJSDebugWin::~KJSDebugWin()
{
  //  detach();
}


KJSDebugWin *KJSDebugWin::createInstance()
{
  assert(!kjs_html_debugger);
  kjs_html_debugger = new KJSDebugWin();
  kjs_html_debugger->show();
  return kjs_html_debugger;
}

void KJSDebugWin::destroyInstance()
{
  assert(kjs_html_debugger);
  kjs_html_debugger->hide();
  delete kjs_html_debugger;
}

KJSDebugWin *KJSDebugWin::instance()
{
  return kjs_html_debugger;
}

void KJSDebugWin::next()
{
  m_mode = Next;
  leaveSession();
}

void KJSDebugWin::step()
{
  m_mode = Step;
  leaveSession();
}

void KJSDebugWin::cont()
{
  m_mode = Continue;
  leaveSession();
}

void KJSDebugWin::stop()
{
  m_mode = Stop;
  leaveSession();
}

void KJSDebugWin::showFrame(int frameno)
{
  StackFrame *frame = m_frames.at(frameno);
  if (!frame)
    return;
  highLight(frame->sourceId,frame->lineno);
}

void KJSDebugWin::sourceSelected(int sourceSelIndex)
{
  // a souce file has been selected from the drop-down list - display the file
  // and hilight the line if it's in the current stack frame
  if (sourceSelIndex < 0 || sourceSelIndex >= (int)m_sourceSel->count())
    return;

  SourceFile *sourceFile = m_sourceSelFiles[sourceSelIndex];
  if (m_curSourceFile != sourceFile)
      setCode(sourceFile->code);
  m_curSourceFile = sourceFile;

  SourceFragment *lastFragment = 0;
  StackFrame *curFrame = m_frames.at(m_frameList->currentItem() >= 0 ? m_frameList->currentItem() : m_frames.count()-1);
  if (curFrame)
    lastFragment = m_sourceFragments[curFrame->sourceId];

  if (lastFragment && lastFragment->sourceFile == m_curSourceFile)
    m_sourceDisplay->setCurrentItem(lastFragment->baseLine+curFrame->lineno);
  else
    m_sourceDisplay->setCurrentItem(-1);
}

void KJSDebugWin::eval()
{
  // evaluate the js code from m_evalEdit

  if (!m_inSession)
    return;

  // create function
  KJS::Constructor constr(KJS::Global::current().get("Function").imp());
  KJS::List args;
  args.append(KJS::String("event"));
  args.append(KJS::String("return "+m_evalEdit->text()+";"));
  KJS::KJSO func = constr.construct(args);
  // execute
  Mode oldMode = m_mode;
  m_mode = Continue; // prevents us from stopping during evaluation

  KJSO ret = m_curContext->executeCall(m_curScript,func,func,0);
  KMessageBox::information(this, ret.toString().value().qstring(), "JavaScript eval");
  m_mode = oldMode;
}

void KJSDebugWin::closeEvent(QCloseEvent *e)
{
  if (m_inSession)
    leaveSession();
  return QWidget::closeEvent(e);
}

bool KJSDebugWin::sourceParsed(KJScript */*script*/, int sourceId,
			       const UString &source, int /*errorLine*/)
{
  // the interpreter has parsed some js code - store it in a SourceFragment object
  // ### report errors (errorLine >= 0)

  SourceFile *sourceFile = m_sourceFiles[m_nextSourceUrl];
  if (!sourceFile) {
    sourceFile = new SourceFile("(unknown)",source.qstring());
    m_sourceSelFiles[m_sourceSel->count()] = sourceFile;
    if (m_nextSourceUrl.isNull() || m_nextSourceUrl == "")
        m_sourceSel->insertItem("???");
    else
        m_sourceSel->insertItem(m_nextSourceUrl);
  }

  SourceFragment *sf = new SourceFragment(sourceId,m_nextSourceBaseLine,sourceFile);
  m_sourceFragments[sourceId] = sf;


  m_nextSourceBaseLine = 0;
  m_nextSourceUrl = "";

  return (m_mode != Stop);
}

bool KJSDebugWin::sourceUnused(KJScript */*script*/, int sourceId)
{
  // the source fragment is no longer in use, so we can free it

  SourceFragment *fragment = m_sourceFragments[sourceId];
  m_sourceFragments.erase(sourceId);
  delete fragment;
  return (m_mode != Stop);
}

bool KJSDebugWin::error(KJScript */*script*/, int /*sourceId*/, int /*lineno*/,
			int /*errorType*/, const KJS::UString &errorMessage)
{
  // ### bring up source & hilight line
  KMessageBox::error(this, errorMessage.qstring(), "JavaScript error");
  return (m_mode != Stop);
}

bool KJSDebugWin::atLine(KJScript *script, int sourceId, int lineno,
			 const ExecutionContext *execContext)
{
  const ExecutionContext *oldCurContext = m_curContext;
  KJScript *oldCurScript = m_curScript;
  m_curContext = execContext;
  m_curScript = script;
  /*
  if (haveBreakpoint(sourceId,lineno)) {
    m_mode = Next;
    m_frames.last()->next = true;
  }
  */

  m_frames.last()->sourceId = sourceId;
  m_frames.last()->lineno = lineno;
  //  highLight(sourceId,lineno);
  if (m_mode == KJSDebugWin::Step || m_mode == KJSDebugWin::Next) {
    if (m_frames.last()->next)
      enterSession();
  }

  m_curContext = oldCurContext;
  m_curScript = oldCurScript;
  return (m_mode != Stop);
}

bool KJSDebugWin::callEvent(KJScript *script, int sourceId, int lineno,
			    const ExecutionContext *execContext,
			    FunctionImp *function, const List */*args*/)
{
  //  highLight(sourceId,lineno);
  const ExecutionContext *oldCurContext = m_curContext;
  KJScript *oldCurScript = m_curScript;
  m_curContext = execContext;
  m_curScript = script;
  QString name = function->name().qstring();
  StackFrame *sf = new StackFrame(sourceId,lineno,name,m_mode == Step);
  m_frames.append(sf);
  if (m_mode == Step)
    enterSession();
  m_curContext = oldCurContext;
  m_curScript = oldCurScript;
  return (m_mode != Stop);
}

bool KJSDebugWin::returnEvent(KJScript *script, int sourceId, int lineno,
			      const ExecutionContext *execContext,
			      FunctionImp */*function*/)
{
  //  highLight(sourceId,lineno);
  const ExecutionContext *oldCurContext = m_curContext;
  KJScript *oldCurScript = m_curScript;
  m_curContext = execContext;
  m_curScript = script;
  m_frames.last()->sourceId = sourceId;
  m_frames.last()->lineno = lineno;
  if (m_frames.last()->step)
    enterSession();
  m_frames.removeLast();
  //  m_frameList->removeItem(m_frameList->count()-1);
  m_curContext = oldCurContext;
  m_curScript = oldCurScript;
  return (m_mode != Stop);
}

void KJSDebugWin::setCode(const QString &code)
{
  const QChar *chars = code.unicode();
  uint len = code.length();
  QChar newLine('\n');
  uint lineStart = 0;
  m_sourceDisplay->clear();
  // ### support for \r\n and \n\r (?)
  for (uint i = 0; i < len; i++) {
    if (chars[i] == newLine) {
      QString line;
      if (lineStart == i)
	line = "";
      else
	line = QString(chars+lineStart,i-lineStart);

      QListBoxPixmap *qbp;
      if (m_sourceDisplay->count() == 5)
	qbp = new QListBoxPixmap(m_stopIcon,line);
      else
	qbp = new QListBoxPixmap(m_emptyIcon,line);
      m_sourceDisplay->insertItem(qbp);

      lineStart = i+1;
    }
  }
}

void KJSDebugWin::highLight(int sourceId, int line)
{
  if (!isVisible())
    show();

  SourceFragment *source = m_sourceFragments[sourceId];
  if (!source)
    return;

  SourceFile *sourceFile = source->sourceFile;
  if (m_curSourceFile != source->sourceFile)
      setCode(sourceFile->code);
  m_curSourceFile = source->sourceFile;

  if (line >= 0)
    m_sourceDisplay->setCurrentItem(line+source->baseLine);
  else
    m_sourceDisplay->setCurrentItem(-1);
}

void KJSDebugWin::setNextSourceInfo(QString url, int baseLine)
{
  m_nextSourceUrl = url;
  m_nextSourceBaseLine = baseLine;
}

void KJSDebugWin::setSourceFile(QString url, QString code)
{
  SourceFile *existing = m_sourceFiles[url];
  if (existing)
    existing->deref();
  SourceFile *newSF = new SourceFile(url,code);
  m_sourceFiles[url] = newSF;
}

void KJSDebugWin::appendSourceFile(QString url, QString code)
{
  SourceFile *existing = m_sourceFiles[url];
  if (!existing) {
    setSourceFile(url,code);
    return;
  }
  existing->code.append(code);
}

void KJSDebugWin::enterSession()
{
  // This "enters" a new debugging session, i.e. enables usage of the debugging window
  // It re-enters the qt event loop here, allowing execution of other parts of the
  // program to continue while the script is stopped. We have to be a bit careful here,
  // i.e. make sure the user can't quite the app, and not executing more js code
  assert(!m_inSession);
  m_fakeModal.enable(this);
  m_inSession = true;
  m_mode = Continue;

  m_nextButton->setEnabled(true);
  m_stepButton->setEnabled(true);
  m_continueButton->setEnabled(true);
  m_stopButton->setEnabled(true);
  m_evalButton->setEnabled(true);
  updateFrameList();

  qApp->enter_loop(); // won't return until leaveSession() is called
  assert(!m_inSession);
}

void KJSDebugWin::leaveSession()
{
  // Disables debugging for this window and returns to execute the rest of the script
  // (or aborts execution, if the user pressed stop). When this returns, the program
  // will exit the qt event loop, i.e. return to whatever processing was being done
  // before the debugger was stopped.
  assert(m_inSession);
  m_nextButton->setEnabled(false);
  m_stepButton->setEnabled(false);
  m_continueButton->setEnabled(false);
  m_stopButton->setEnabled(false);
  m_evalButton->setEnabled(false);
  m_inSession = false;
  qApp->exit_loop();
  m_fakeModal.disable();
}

void KJSDebugWin::updateFrameList()
{
  uint frameno;
  //  m_frameList->setUpdatesEnabled(false);
  m_frameList->clear();
  for (frameno = 0; frameno < m_frames.count(); frameno++) {
    m_frameList->insertItem(m_frames.at(frameno)->toString(),frameno);
  }
  //  m_frameList->setUpdatesEnabled(true);
  //  m_frameList->triggerUpdate(true);
  highLight(m_frames.last()->sourceId,m_frames.last()->lineno);
}

bool KJSDebugWin::setBreakpoint(int sourceId, int line)
{
  if (haveBreakpoint(sourceId,line,line))
    return false;

  SourceBreakpoints *sbp = m_sourceBreakpoints;
  while(sbp && sbp->sourceId != sourceId)
    sbp = sbp->next;
  if (!sbp) {
    sbp = new SourceBreakpoints;
    sbp->sourceId = sourceId;
    sbp->breakpoints = 0;
    sbp->next = m_sourceBreakpoints;
    m_sourceBreakpoints = sbp;
  }

  Breakpoint *newbp = new Breakpoint;
  newbp->lineno = line;
  newbp->next = sbp->breakpoints;
  sbp->breakpoints = newbp;

  return true;
}

bool KJSDebugWin::deleteBreakpoint(int sourceId, int line)
{
  for (SourceBreakpoints *sbp = m_sourceBreakpoints; sbp; sbp = sbp->next) {
    if (sbp->sourceId == sourceId) {
      // found breakpoints for this sourceId
      Breakpoint *bp = sbp->breakpoints;
      if (bp && bp->lineno == line) {
	// was the first breakpoint
	Breakpoint *next = bp->next;
	delete bp;
	bp = next;
	return true;
      }

      while (bp->next && bp->next->lineno != line)
	bp = bp->next;
      if (bp->next && bp->next->lineno == line) {
	// found at subsequent breakpoint
	Breakpoint *next = bp->next;
	delete bp;
	bp = next;
	return true;
      }
      return false;
    }
  }
  // no breakpoints at all for this sourceId
  return false;
}

void KJSDebugWin::clearAllBreakpoints(int sourceId)
{
  SourceBreakpoints *nextsbp = 0;
  for (SourceBreakpoints *sbp = m_sourceBreakpoints; sbp; sbp = nextsbp) {
    nextsbp = sbp->next;
    if (sourceId == -1 || sbp->sourceId == sourceId) {
      Breakpoint *nextbp;
      for (Breakpoint *bp = sbp->breakpoints; bp; bp = bp->next) {
	nextbp = bp->next;
	delete bp;
      }
      delete sbp;
    }
  }
}

int KJSDebugWin::breakpointLine(int sourceId, int line0, int line1)
{
  for (SourceBreakpoints *sbp = m_sourceBreakpoints; sbp; sbp = sbp->next) {
    if (sbp->sourceId == sourceId) {
      // found breakpoints for this sourceId
      for (Breakpoint *bp = sbp->breakpoints; bp; bp = bp->next) {
	if (bp->lineno >= 0 && bp->lineno >= line0 && bp->lineno <= line1)
	  return bp->lineno;
      }
      return -1;
    }
  }
  // no breakpoints at all for this sourceId
  return -1;
}

bool KJSDebugWin::haveBreakpoint(int sourceId, int line0, int line1)
{
  return (breakpointLine(sourceId,line0,line1) != -1);
}

#include "kjs_debugwin.moc"

#endif // KJS_DEBUGGER

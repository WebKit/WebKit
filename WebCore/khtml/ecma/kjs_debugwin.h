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

#ifndef _KJS_DEBUGGER_H_
#define _KJS_DEBUGGER_H_

#include <qglobal.h>

#include <config.h>

#ifndef APPLE_CHANGES
#ifdef __GNUC__
#warning TODO port the debugger
#endif
#endif


//#define KJS_DEBUGGER

#ifdef KJS_DEBUGGER

#include <qwidget.h>
#include <qpixmap.h>
#include <qlist.h>

#include <kjs/debugger.h>

#include "dom/dom_misc.h"

class QListBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class KJScript;

namespace KJS {
  class FunctionImp;
  class List;
};

/**
 * @internal
 *
 * A hack to allow KJSDebugWin to act as a modal window only some of the time
 * (i.e. when it is paused, to prevent the user quitting the app during a
 * debugging session)
 */
class FakeModal : public QWidget
{
    Q_OBJECT
public:
    FakeModal() {}
    void enable(QWidget *modal);
    void disable();

protected:
    bool eventFilter( QObject *obj, QEvent *evt );
    QWidget *modalWidget;
};


/**
 * @internal
 *
 * Represents a frame on the execution stack. The top frame is the global code for
 * the script, and each frame below it represents a function call.
 */
class StackFrame {
 public:
  StackFrame(int s, int l, QString n, bool stepped)
    : sourceId(s), lineno(l), name(n), step(stepped),
    next(stepped) {}
  QString toString();

  int sourceId;
  int lineno;
  QString name;
  bool step;
  bool next;
};

class SourceFile : public DOM::DomShared {
 public:
  SourceFile(QString u, QString c) : url(u), code(c) {}
  QString url;
  QString code;
};

/**
 * @internal
 *
 * When kjs parses some code, it generates a source code fragment (or just "source").
 * This is referenced by it's source id in future calls to functions such as atLine()
 * and callEvent(). We keep a record of all source fragments parsed in order to display
 * then to the user.
 *
 * For .js files, the source fragment will be the entire file. For js code included
 * in html files, however, there may be multiple source fragments within the one file
 * (e.g. multiple SCRIPT tags or onclick="..." attributes)
 *
 * In the case where a single file has multiple source fragments, the source objects
 * for these fragments will all point to the same SourceFile for their code.
 */
class SourceFragment {
 public:
  SourceFragment(int sid, int bl, SourceFile *sf);
  ~SourceFragment();

  int sourceId;
  int baseLine;
  SourceFile *sourceFile;
};

/**
 * @internal
 *
 * KJSDebugWin represents the debugger window that is visible to the user. It contains
 * a stack frame list, a code viewer and a source fragment selector, plus buttons
 * to control execution including next, step and continue.
 *
 * There is only one debug window per program. This can be obtained by calling #instance
 */
class KJSDebugWin : public QWidget, public KJS::Debugger {
  Q_OBJECT
public:
  KJSDebugWin(QWidget *parent=0, const char *name=0);
  virtual ~KJSDebugWin();

  static KJSDebugWin *createInstance();
  static void destroyInstance();
  static KJSDebugWin *instance();

  enum Mode { Disabled = 0, // No break on any statements
	      Next     = 1, // Will break on next statement in current context
	      Step     = 2, // Will break on next statement in current or deeper context
	      Continue = 3, // Will continue until next breakpoint
	      Stop     = 4  // The script will stop execution completely,
	                    // as soon as possible
  };

  void highLight(int sourceId, int line);
  void setNextSourceInfo(QString url, int baseLine);
  void setSourceFile(QString url, QString code);
  void appendSourceFile(QString url, QString code);
  bool inSession() const { return m_inSession; }
  KJScript *currentScript() { return m_curScript; }
  void setMode(Mode m) { m_mode = m; }

public slots:
  void next();
  void step();
  void cont();
  void stop();
  void showFrame(int frameno);
  void sourceSelected(int sourceSelIndex);
  void eval();

protected:

  virtual void closeEvent(QCloseEvent *e);

  // functions overridden from KJS:Debugger
  virtual bool sourceParsed(KJScript *script, int sourceId,
			    const KJS::UString &source, int errorLine);

  virtual bool sourceUnused(KJScript *script, int sourceId);

  virtual bool error(KJScript *script, int sourceId, int lineno,
		     int errorType, const KJS::UString &errorMessage);

  virtual bool atLine(KJScript *script, int sourceId, int lineno,
		      const KJS::ExecState *execContext);

  virtual bool callEvent(KJScript *script, int sourceId, int lineno,
			 const KJS::ExecState *execContext,
			 KJS::FunctionImp *function, const KJS::List *args);

  virtual bool returnEvent(KJScript *script, int sourceId, int lineno,
			   const KJS::ExecState *execContext,
			   KJS::FunctionImp *function);

private:
  void enterSession();
  void leaveSession();
  void setCode(const QString &code);
  void updateFrameList();

  struct Breakpoint {
    int lineno;
    Breakpoint *next;
  };
  struct SourceBreakpoints {
    int sourceId;
    Breakpoint *breakpoints;
    SourceBreakpoints *next;
  };
  SourceBreakpoints *m_sourceBreakpoints;

  bool setBreakpoint(int sourceId, int line);
  bool deleteBreakpoint(int sourceId, int line);
  void clearAllBreakpoints(int sourceId = -1);
  int breakpointLine(int sourceId, int line0, int line1);
  bool haveBreakpoint(int sourceId, int line0, int line1);

  bool m_inSession;

  QListBox *m_sourceDisplay;
  QListBox *m_frameList;
  QPushButton *m_stepButton;
  QPushButton *m_nextButton;
  QPushButton *m_continueButton;
  QPushButton *m_stopButton;
  QComboBox *m_sourceSel;
  QPixmap m_stopIcon;
  QPixmap m_emptyIcon;
  QLineEdit *m_evalEdit;
  QPushButton *m_evalButton;

  SourceFile *m_curSourceFile;
  QList<StackFrame> m_frames;
  Mode m_mode;
  QMap<QString,SourceFile*> m_sourceFiles;
  QMap<int,SourceFragment*> m_sourceFragments;
  QMap<int,SourceFile*> m_sourceSelFiles;

  QString m_nextSourceUrl;
  int m_nextSourceBaseLine;
  FakeModal m_fakeModal;
  //const KJS::ExecutionContext *m_curContext;
  KJScript *m_curScript;
};

#endif // KJS_DEBUGGER

#endif // _KJS_DEBUGGER_H_

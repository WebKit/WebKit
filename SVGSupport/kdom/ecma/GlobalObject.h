/*
    Copyright (C) 2002 David Faure <faure@kde.org>
                     2004, 2005 Nikolas Zimmermann <wildfox@kde.org>

    This file is part of the KDE project

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_GlobalObject_H
#define KDOM_GlobalObject_H

#include <qmap.h>
#include <qobject.h>
#include <qguardedptr.h>

#include <kjs/object.h>

namespace KDOM
{
    class DocumentImpl;
    class GlobalObject : public KJS::ObjectImp
    {
    public:
        GlobalObject(DocumentImpl *doc);
        virtual ~GlobalObject();

        virtual KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const;
        virtual void put(KJS::ExecState *exec, const KJS::Identifier &propertyName, KJS::ValueImp *value, int attr = KJS::None);
        virtual bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &p) const;

        // Returns a pointer to the Global Object in this EcmaScript Context
        static GlobalObject *retrieveActive(KJS::ExecState *exec);

        DocumentImpl *doc() const;

        // Timer related stuff
        int installTimeout(const KJS::UString &handler, int t, bool singleShot);
        void clearTimeout(int timerId);

        // Helper functions
        bool isSafeScript(KJS::ExecState *exec) const;
        void clear(KJS::ExecState *exec);
    
        // EcmaScript specific stuff - only needed for GlobalObject
        // You won't find it in "general" kdom ecma code...
        virtual const KJS::ClassInfo *classInfo() const;

        static const KJS::ClassInfo s_classInfo;
        static const struct KJS::HashTable s_hashTable;

        // Ecma updating logic
        virtual void afterTimeout() const { }

        enum
        {
            // Attributes
            Closed, Window, Evt, Document,

            // Functions
            SetTimeout, ClearTimeout, SetInterval,
            ClearInterval, PrintNode, Alert, Prompt,
            Confirm, Debug,

            // Constants
            Event, MutationEvent, EventException,
            CSSRule, CSSValue, CSSPrimitiveValue,
            Node, TypeInfo, DOMError, DOMException,
            Range, RangeException, NodeFilter, XPathResult,
            XPathException, XPathNamespace, XPointerResult

        };

    private:
        class Private;
        Private *d;
    };

    // Internal classes! Do not use as 3rd party developer!
    class ScheduledAction
    {
    public:
        ScheduledAction(KJS::ObjectImp *func, KJS::List args, bool singleShot);
        ScheduledAction(const QString &code, bool singleShot);
        ~ScheduledAction();
    
        void execute(GlobalObject *global);

    private:
        friend class GlobalQObject;

        KJS::ObjectImp *m_func;
        KJS::List m_args;
        QString m_code;
        bool m_isFunction;
        bool m_singleShot;
    };

    class GlobalQObject : public QObject
    {
    Q_OBJECT
    public:
        GlobalQObject(GlobalObject *obj);
        ~GlobalQObject();

        int installTimeout(const KJS::UString &handler, int t, bool singleShot);
        int installTimeout(KJS::ValueImp *func, KJS::List args, int t, bool singleShot);
        void clearTimeout(int timerId, bool delAction = true);
        
    public slots:
        void timeoutClose();

    protected slots:
        void parentDestroyed();

    protected:
        void timerEvent(QTimerEvent *e);

    private:
        GlobalObject *parent;
        QMap<int, ScheduledAction *> scheduledActions;
    };
};

#endif

// vim:ts=4:noet

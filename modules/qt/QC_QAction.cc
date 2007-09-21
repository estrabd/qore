/*
 QC_QAction.cc
 
 Qore Programming Language
 
 Copyright (C) 2003, 2004, 2005, 2006, 2007 David Nichols
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <qore/Qore.h>

#include "QC_QAction.h"

int CID_QACTION;
class QoreClass *QC_QAction = 0;

//QAction ( QObject * parent )
//QAction ( const QString & text, QObject * parent )
//QAction ( const QIcon & icon, const QString & text, QObject * parent )
static void QACTION_constructor(Object *self, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   if (p && p->type == NT_OBJECT) {
      QoreQIcon *icon = (QoreQIcon *)p->val.object->getReferencedPrivateData(CID_QICON, xsink);
      if (!icon) {
         QoreAbstractQObject *parent = (QoreAbstractQObject *)p->val.object->getReferencedPrivateData(CID_QOBJECT, xsink);
         if (!parent) {
            if (!xsink->isException())
               xsink->raiseException("QACTION-CONSTRUCTOR-PARAM-ERROR", "QAction::constructor() does not know how to handle arguments of class '%s' as passed as the first argument", p->val.object->getClass()->getName());
            return;
         }
         ReferenceHolder<AbstractPrivateData> parentHolder(static_cast<AbstractPrivateData *>(parent), xsink);
         self->setPrivate(CID_QACTION, new QoreQAction(self, parent->getQObject()));
         return;
      }
      ReferenceHolder<AbstractPrivateData> iconHolder(static_cast<AbstractPrivateData *>(icon), xsink);
      p = get_param(params, 1);
      QString text;
      if (get_qstring(p, text, xsink))
         return;
      p = get_param(params, 2);
      QoreAbstractQObject *parent = (p && p->type == NT_OBJECT) ? (QoreAbstractQObject *)p->val.object->getReferencedPrivateData(CID_QOBJECT, xsink) : 0;
      if (!parent) {
         if (!xsink->isException())
            xsink->raiseException("QACTION-CONSTRUCTOR-PARAM-ERROR", "this version of QAction::constructor() expects an object derived from QObject as the third argument", p->val.object->getClass()->getName());
         return;
      }
      ReferenceHolder<AbstractPrivateData> parentHolder(static_cast<AbstractPrivateData *>(parent), xsink);
      self->setPrivate(CID_QACTION, new QoreQAction(self, *(static_cast<QIcon *>(icon)), text, parent->getQObject()));
      return;
   }
   QString text;
   if (get_qstring(p, text, xsink))
      return;
   p = get_param(params, 1);
   QoreAbstractQObject *parent = (p && p->type == NT_OBJECT) ? (QoreAbstractQObject *)p->val.object->getReferencedPrivateData(CID_QOBJECT, xsink) : 0;
   if (!parent) {
      if (!xsink->isException())
         xsink->raiseException("QACTION-CONSTRUCTOR-PARAM-ERROR", "this version of QAction::constructor() expects an object derived from QObject as the second argument", p->val.object->getClass()->getName());
      return;
   }
   ReferenceHolder<AbstractPrivateData> parentHolder(static_cast<AbstractPrivateData *>(parent), xsink);
   self->setPrivate(CID_QACTION, new QoreQAction(self, text, parent->getQObject()));
   return;
}

static void QACTION_copy(class Object *self, class Object *old, class QoreQAction *qa, ExceptionSink *xsink)
{
   xsink->raiseException("QACTION-COPY-ERROR", "objects of this class cannot be copied");
}

//QActionGroup * actionGroup () const
static QoreNode *QACTION_actionGroup(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QActionGroup *qt_qobj = qa->getQAction()->actionGroup();
   if (!qt_qobj)
      return 0;
   QVariant qv_ptr = qt_qobj->property("qobject");
   Object *rv_obj = reinterpret_cast<Object *>(qv_ptr.toULongLong());
   assert(rv_obj);
   rv_obj->ref();
   return new QoreNode(rv_obj);
}

//void activate ( ActionEvent event )
static QoreNode *QACTION_activate(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QAction::ActionEvent event = (QAction::ActionEvent)(p ? p->getAsInt() : 0);
   qa->getQAction()->activate(event);
   return 0;
}

////QList<QWidget *> associatedWidgets () const
//static QoreNode *QACTION_associatedWidgets(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
//{
//   ??? return new QoreNode((int64)qa->getQAction()->associatedWidgets());
//}

//bool autoRepeat () const
static QoreNode *QACTION_autoRepeat(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->autoRepeat());
}

//QVariant data () const
static QoreNode *QACTION_data(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return return_qvariant(qa->getQAction()->data());
}

//QFont font () const
static QoreNode *QACTION_font(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   Object *o_qf = new Object(QC_QFont, getProgram());
   QoreQFont *q_qf = new QoreQFont(qa->getQAction()->font());
   o_qf->setPrivate(CID_QFONT, q_qf);
   return new QoreNode(o_qf);
}

//QIcon icon () const
static QoreNode *QACTION_icon(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   Object *o_qi = new Object(QC_QIcon, getProgram());
   QoreQIcon *q_qi = new QoreQIcon(qa->getQAction()->icon());
   o_qi->setPrivate(CID_QICON, q_qi);
   return new QoreNode(o_qi);
}

//QString iconText () const
static QoreNode *QACTION_iconText(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(new QoreString(qa->getQAction()->iconText().toUtf8().data(), QCS_UTF8));
}

//bool isCheckable () const
static QoreNode *QACTION_isCheckable(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->isCheckable());
}

//bool isChecked () const
static QoreNode *QACTION_isChecked(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->isChecked());
}

//bool isEnabled () const
static QoreNode *QACTION_isEnabled(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->isEnabled());
}

//bool isSeparator () const
static QoreNode *QACTION_isSeparator(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->isSeparator());
}

//bool isVisible () const
static QoreNode *QACTION_isVisible(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qa->getQAction()->isVisible());
}

//QMenu * menu () const
static QoreNode *QACTION_menu(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QMenu *qt_qobj = qa->getQAction()->menu();
   if (!qt_qobj)
      return 0;
   QVariant qv_ptr = qt_qobj->property("qobject");
   Object *rv_obj = reinterpret_cast<Object *>(qv_ptr.toULongLong());
   assert(rv_obj);
   rv_obj->ref();
   return new QoreNode(rv_obj);
}

//MenuRole menuRole () const
static QoreNode *QACTION_menuRole(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((int64)qa->getQAction()->menuRole());
}

//QWidget * parentWidget () const
static QoreNode *QACTION_parentWidget(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QWidget *qt_qobj = qa->getQAction()->parentWidget();
   if (!qt_qobj)
      return 0;
   QVariant qv_ptr = qt_qobj->property("qobject");
   Object *rv_obj = reinterpret_cast<Object *>(qv_ptr.toULongLong());
   assert(rv_obj);
   rv_obj->ref();
   return new QoreNode(rv_obj);
}

//void setActionGroup ( QActionGroup * group )
static QoreNode *QACTION_setActionGroup(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQActionGroup *group = (p && p->type == NT_OBJECT) ? (QoreQActionGroup *)p->val.object->getReferencedPrivateData(CID_QACTIONGROUP, xsink) : 0;
   if (!group) {
      if (!xsink->isException())
         xsink->raiseException("QACTION-SETACTIONGROUP-PARAM-ERROR", "expecting a QActionGroup object as first argument to QAction::setActionGroup()");
      return 0;
   }
   ReferenceHolder<AbstractPrivateData> groupHolder(static_cast<AbstractPrivateData *>(group), xsink);
   qa->getQAction()->setActionGroup(static_cast<QActionGroup *>(group->qobj));
   return 0;
}

//void setAutoRepeat ( bool )
static QoreNode *QACTION_setAutoRepeat(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setAutoRepeat(b);
   return 0;
}

//void setCheckable ( bool )
static QoreNode *QACTION_setCheckable(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setCheckable(b);
   return 0;
}

//void setData ( const QVariant & userData )
static QoreNode *QACTION_setData(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QVariant userData;
   if (get_qvariant(p, userData, xsink))
      return 0;
   qa->getQAction()->setData(userData);
   return 0;
}

//void setFont ( const QFont & font )
static QoreNode *QACTION_setFont(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQFont *font = (p && p->type == NT_OBJECT) ? (QoreQFont *)p->val.object->getReferencedPrivateData(CID_QFONT, xsink) : 0;
   if (!font) {
      if (!xsink->isException())
         xsink->raiseException("QACTION-SETFONT-PARAM-ERROR", "expecting a QFont object as first argument to QAction::setFont()");
      return 0;
   }
   ReferenceHolder<AbstractPrivateData> fontHolder(static_cast<AbstractPrivateData *>(font), xsink);
   qa->getQAction()->setFont(*(static_cast<QFont *>(font)));
   return 0;
}

//void setIcon ( const QIcon & icon )
static QoreNode *QACTION_setIcon(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQIcon *icon = (p && p->type == NT_OBJECT) ? (QoreQIcon *)p->val.object->getReferencedPrivateData(CID_QICON, xsink) : 0;
   if (!icon) {
      if (!xsink->isException())
         xsink->raiseException("QACTION-SETICON-PARAM-ERROR", "expecting a QIcon object as first argument to QAction::setIcon()");
      return 0;
   }
   ReferenceHolder<AbstractPrivateData> iconHolder(static_cast<AbstractPrivateData *>(icon), xsink);
   qa->getQAction()->setIcon(*(static_cast<QIcon *>(icon)));
   return 0;
}

//void setIconText ( const QString & text )
static QoreNode *QACTION_setIconText(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QString text;
   if (get_qstring(p, text, xsink))
      return 0;
   qa->getQAction()->setIconText(text);
   return 0;
}

//void setMenu ( QMenu * menu )
static QoreNode *QACTION_setMenu(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreAbstractQMenu *menu = (p && p->type == NT_OBJECT) ? (QoreAbstractQMenu *)p->val.object->getReferencedPrivateData(CID_QMENU, xsink) : 0;
   if (!menu) {
      if (!xsink->isException())
         xsink->raiseException("QACTION-SETMENU-PARAM-ERROR", "expecting a QMenu object as first argument to QAction::setMenu()");
      return 0;
   }
   ReferenceHolder<AbstractPrivateData> menuHolder(static_cast<AbstractPrivateData *>(menu), xsink);
   qa->getQAction()->setMenu(menu->getQMenu());
   return 0;
}

//void setMenuRole ( MenuRole menuRole )
static QoreNode *QACTION_setMenuRole(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QAction::MenuRole menuRole = (QAction::MenuRole)(p ? p->getAsInt() : 0);
   qa->getQAction()->setMenuRole(menuRole);
   return 0;
}

//void setSeparator ( bool b )
static QoreNode *QACTION_setSeparator(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setSeparator(b);
   return 0;
}

//void setShortcut ( const QKeySequence & shortcut )
static QoreNode *QACTION_setShortcut(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);

   QKeySequence shortcut;
   if (get_qkeysequence(p, shortcut, xsink))
      return 0;

   qa->getQAction()->setShortcut(shortcut);
   return 0;
}

//void setShortcutContext ( Qt::ShortcutContext context )
static QoreNode *QACTION_setShortcutContext(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   Qt::ShortcutContext context = (Qt::ShortcutContext)(p ? p->getAsInt() : 0);
   qa->getQAction()->setShortcutContext(context);
   return 0;
}

//void setShortcuts ( const QList<QKeySequence> & shortcuts )
//void setShortcuts ( QKeySequence::StandardKey key )
static QoreNode *QACTION_setShortcuts(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);

   if (p && p->type == NT_LIST) {
      QList<QKeySequence> shortcuts;

      ListIterator li(p->val.list);
      while (li.next()) {
	 QKeySequence ks;
	 if (get_qkeysequence(li.getValue(), ks, xsink))
	    return 0;
	 shortcuts.push_back(ks);
      }

      qa->getQAction()->setShortcuts(shortcuts);
      return 0;
   }
   QKeySequence::StandardKey key = (QKeySequence::StandardKey)(p ? p->getAsInt() : 0);

   qa->getQAction()->setShortcuts(key);
   return 0;
}

//void setStatusTip ( const QString & statusTip )
static QoreNode *QACTION_setStatusTip(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QString statusTip;
   if (get_qstring(p, statusTip, xsink))
      return 0;
   qa->getQAction()->setStatusTip(statusTip);
   return 0;
}

//void setText ( const QString & text )
static QoreNode *QACTION_setText(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QString text;
   if (get_qstring(p, text, xsink))
      return 0;
   qa->getQAction()->setText(text);
   return 0;
}

//void setToolTip ( const QString & tip )
static QoreNode *QACTION_setToolTip(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QString tip;
   if (get_qstring(p, tip, xsink))
      return 0;
   qa->getQAction()->setToolTip(tip);
   return 0;
}

//void setWhatsThis ( const QString & what )
static QoreNode *QACTION_setWhatsThis(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QString what;
   if (get_qstring(p, what, xsink))
      return 0;
   qa->getQAction()->setWhatsThis(what);
   return 0;
}

//QKeySequence shortcut () const
static QoreNode *QACTION_shortcut(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   Object *o_qks = new Object(QC_QKeySequence, getProgram());
   QoreQKeySequence *q_qks = new QoreQKeySequence(qa->getQAction()->shortcut());
   o_qks->setPrivate(CID_QKEYSEQUENCE, q_qks);
   return new QoreNode(o_qks);
}

//Qt::ShortcutContext shortcutContext () const
static QoreNode *QACTION_shortcutContext(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((int64)qa->getQAction()->shortcutContext());
}

//QList<QKeySequence> shortcuts () const
static QoreNode *QACTION_shortcuts(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QList<QKeySequence> qsl = qa->getQAction()->shortcuts();
   List *l = new List();
   for (QList<QKeySequence>::iterator i = qsl.begin(), e = qsl.end(); i != e; ++i) {
      Object *o_qks = new Object(QC_QKeySequence, getProgram());
      QoreQKeySequence *q_qks = new QoreQKeySequence(*i);
      o_qks->setPrivate(CID_QKEYSEQUENCE, q_qks);
      l->push(new QoreNode(o_qks));
   }
   return new QoreNode(l);
}

//bool showStatusText ( QWidget * widget = 0 )
static QoreNode *QACTION_showStatusText(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQWidget *widget = (p && p->type == NT_OBJECT) ? (QoreQWidget *)p->val.object->getReferencedPrivateData(CID_QWIDGET, xsink) : 0;
   if (*xsink)
      return 0;
   ReferenceHolder<AbstractPrivateData> widgetHolder(static_cast<AbstractPrivateData *>(widget), xsink);
   return new QoreNode(qa->getQAction()->showStatusText(widget ? static_cast<QWidget *>(widget->getQWidget()) : 0));
}

//QString statusTip () const
static QoreNode *QACTION_statusTip(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(new QoreString(qa->getQAction()->statusTip().toUtf8().data(), QCS_UTF8));
}

//QString text () const
static QoreNode *QACTION_text(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(new QoreString(qa->getQAction()->text().toUtf8().data(), QCS_UTF8));
}

//QString toolTip () const
static QoreNode *QACTION_toolTip(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(new QoreString(qa->getQAction()->toolTip().toUtf8().data(), QCS_UTF8));
}

//QString whatsThis () const
static QoreNode *QACTION_whatsThis(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(new QoreString(qa->getQAction()->whatsThis().toUtf8().data(), QCS_UTF8));
}

//void hover ()
static QoreNode *QACTION_hover(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   qa->getQAction()->hover();
   return 0;
}

//void setChecked ( bool )
static QoreNode *QACTION_setChecked(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setChecked(b);
   return 0;
}

//void setDisabled ( bool b )
static QoreNode *QACTION_setDisabled(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setDisabled(b);
   return 0;
}

//void setEnabled ( bool )
static QoreNode *QACTION_setEnabled(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setEnabled(b);
   return 0;
}

//void setVisible ( bool )
static QoreNode *QACTION_setVisible(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qa->getQAction()->setVisible(b);
   return 0;
}

//void toggle ()
static QoreNode *QACTION_toggle(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   qa->getQAction()->toggle();
   return 0;
}

//void trigger ()
static QoreNode *QACTION_trigger(Object *self, QoreAbstractQAction *qa, QoreNode *params, ExceptionSink *xsink)
{
   qa->getQAction()->trigger();
   return 0;
}

QoreClass *initQActionClass(QoreClass *qobject)
{
   QC_QAction = new QoreClass("QAction", QDOM_GUI);
   CID_QACTION = QC_QAction->getID();

   QC_QAction->addBuiltinVirtualBaseClass(qobject);

   QC_QAction->setConstructor(QACTION_constructor);
   QC_QAction->setCopy((q_copy_t)QACTION_copy);

   QC_QAction->addMethod("actionGroup",                 (q_method_t)QACTION_actionGroup);
   QC_QAction->addMethod("activate",                    (q_method_t)QACTION_activate);
   //QC_QAction->addMethod("associatedWidgets",           (q_method_t)QACTION_associatedWidgets);
   QC_QAction->addMethod("autoRepeat",                  (q_method_t)QACTION_autoRepeat);
   QC_QAction->addMethod("data",                        (q_method_t)QACTION_data);
   QC_QAction->addMethod("font",                        (q_method_t)QACTION_font);
   QC_QAction->addMethod("icon",                        (q_method_t)QACTION_icon);
   QC_QAction->addMethod("iconText",                    (q_method_t)QACTION_iconText);
   QC_QAction->addMethod("isCheckable",                 (q_method_t)QACTION_isCheckable);
   QC_QAction->addMethod("isChecked",                   (q_method_t)QACTION_isChecked);
   QC_QAction->addMethod("isEnabled",                   (q_method_t)QACTION_isEnabled);
   QC_QAction->addMethod("isSeparator",                 (q_method_t)QACTION_isSeparator);
   QC_QAction->addMethod("isVisible",                   (q_method_t)QACTION_isVisible);
   QC_QAction->addMethod("menu",                        (q_method_t)QACTION_menu);
   QC_QAction->addMethod("menuRole",                    (q_method_t)QACTION_menuRole);
   QC_QAction->addMethod("parentWidget",                (q_method_t)QACTION_parentWidget);
   QC_QAction->addMethod("setActionGroup",              (q_method_t)QACTION_setActionGroup);
   QC_QAction->addMethod("setAutoRepeat",               (q_method_t)QACTION_setAutoRepeat);
   QC_QAction->addMethod("setCheckable",                (q_method_t)QACTION_setCheckable);
   QC_QAction->addMethod("setData",                     (q_method_t)QACTION_setData);
   QC_QAction->addMethod("setFont",                     (q_method_t)QACTION_setFont);
   QC_QAction->addMethod("setIcon",                     (q_method_t)QACTION_setIcon);
   QC_QAction->addMethod("setIconText",                 (q_method_t)QACTION_setIconText);
   QC_QAction->addMethod("setMenu",                     (q_method_t)QACTION_setMenu);
   QC_QAction->addMethod("setMenuRole",                 (q_method_t)QACTION_setMenuRole);
   QC_QAction->addMethod("setSeparator",                (q_method_t)QACTION_setSeparator);
   QC_QAction->addMethod("setShortcut",                 (q_method_t)QACTION_setShortcut);
   QC_QAction->addMethod("setShortcutContext",          (q_method_t)QACTION_setShortcutContext);
   QC_QAction->addMethod("setShortcuts",                (q_method_t)QACTION_setShortcuts);
   QC_QAction->addMethod("setStatusTip",                (q_method_t)QACTION_setStatusTip);
   QC_QAction->addMethod("setText",                     (q_method_t)QACTION_setText);
   QC_QAction->addMethod("setToolTip",                  (q_method_t)QACTION_setToolTip);
   QC_QAction->addMethod("setWhatsThis",                (q_method_t)QACTION_setWhatsThis);
   QC_QAction->addMethod("shortcut",                    (q_method_t)QACTION_shortcut);
   QC_QAction->addMethod("shortcutContext",             (q_method_t)QACTION_shortcutContext);
   QC_QAction->addMethod("shortcuts",                   (q_method_t)QACTION_shortcuts);
   QC_QAction->addMethod("showStatusText",              (q_method_t)QACTION_showStatusText);
   QC_QAction->addMethod("statusTip",                   (q_method_t)QACTION_statusTip);
   QC_QAction->addMethod("text",                        (q_method_t)QACTION_text);
   QC_QAction->addMethod("toolTip",                     (q_method_t)QACTION_toolTip);
   QC_QAction->addMethod("whatsThis",                   (q_method_t)QACTION_whatsThis);
   QC_QAction->addMethod("hover",                       (q_method_t)QACTION_hover);
   QC_QAction->addMethod("setChecked",                  (q_method_t)QACTION_setChecked);
   QC_QAction->addMethod("setDisabled",                 (q_method_t)QACTION_setDisabled);
   QC_QAction->addMethod("setEnabled",                  (q_method_t)QACTION_setEnabled);
   QC_QAction->addMethod("setVisible",                  (q_method_t)QACTION_setVisible);
   QC_QAction->addMethod("toggle",                      (q_method_t)QACTION_toggle);
   QC_QAction->addMethod("trigger",                     (q_method_t)QACTION_trigger);

   return QC_QAction;
}

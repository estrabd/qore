/*
 QC_QAbstractItemDelegate.cc
 
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

#include "QC_QAbstractItemDelegate.h"

int CID_QABSTRACTITEMDELEGATE;
class QoreClass *QC_QAbstractItemDelegate = 0;

//QAbstractItemDelegate ( QObject * parent = 0 )
static void QABSTRACTITEMDELEGATE_constructor(Object *self, QoreNode *params, ExceptionSink *xsink)
{
   xsink->raiseException("QABSTRACTITEMDELEGATE-CONSTRUCTOR-ERROR", "QAbstractItemDelegate is an abstract class");
   return;
}

static void QABSTRACTITEMDELEGATE_copy(class Object *self, class Object *old, class QoreAbstractQAbstractItemDelegate *qaid, ExceptionSink *xsink)
{
   xsink->raiseException("QABSTRACTITEMDELEGATE-COPY-ERROR", "objects of this class cannot be copied");
}

//virtual QWidget * createEditor ( QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index ) const
static QoreNode *QABSTRACTITEMDELEGATE_createEditor(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreAbstractQWidget *parent = (p && p->type == NT_OBJECT) ? (QoreAbstractQWidget *)p->val.object->getReferencedPrivateData(CID_QWIDGET, xsink) : 0;
   if (!parent) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-CREATEEDITOR-PARAM-ERROR", "expecting a QWidget object as first argument to QAbstractItemDelegate::createEditor()");
      return 0;
   }
   ReferenceHolder<QoreAbstractQWidget> parentHolder(parent, xsink);
   p = get_param(params, 1);
   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
   if (!option) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-CREATEEDITOR-PARAM-ERROR", "expecting a QStyleOptionViewItem object as second argument to QAbstractItemDelegate::createEditor()");
      return 0;
   }
   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
   p = get_param(params, 2);
   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
   if (!index) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-CREATEEDITOR-PARAM-ERROR", "expecting a QModelIndex object as third argument to QAbstractItemDelegate::createEditor()");
      return 0;
   }
   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
   QObject *qt_qobj = qaid->getQAbstractItemDelegate()->createEditor(parent->getQWidget(), *(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index)));
   QVariant qv_ptr = qt_qobj->property("qobject");
   Object *rv_obj = reinterpret_cast<Object *>(qv_ptr.toULongLong());
   assert(rv_obj);
   rv_obj->ref();
   return new QoreNode(rv_obj);
}

////virtual bool editorEvent ( QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index )
//static QoreNode *QABSTRACTITEMDELEGATE_editorEvent(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
//{
//   QoreNode *p = get_param(params, 0);
//   ??? QEvent* event = p;
//   p = get_param(params, 1);
//   ??? QAbstractItemModel* model = p;
//   p = get_param(params, 2);
//   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
//   if (!option) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-EDITOREVENT-PARAM-ERROR", "expecting a QStyleOptionViewItem object as third argument to QAbstractItemDelegate::editorEvent()");
//      return 0;
//   }
//   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
//   p = get_param(params, 3);
//   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
//   if (!index) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-EDITOREVENT-PARAM-ERROR", "expecting a QModelIndex object as fourth argument to QAbstractItemDelegate::editorEvent()");
//      return 0;
//   }
//   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
//   return new QoreNode(qaid->getQAbstractItemDelegate()->editorEvent(event, model, *(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index))));
//}

//virtual void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const = 0
static QoreNode *QABSTRACTITEMDELEGATE_paint(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQPainter *painter = (p && p->type == NT_OBJECT) ? (QoreQPainter *)p->val.object->getReferencedPrivateData(CID_QPAINTER, xsink) : 0;
   if (!painter) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-PAINT-PARAM-ERROR", "expecting a QPainter object as first argument to QAbstractItemDelegate::paint()");
      return 0;
   }
   ReferenceHolder<QoreQPainter> painterHolder(painter, xsink);
   p = get_param(params, 1);
   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
   if (!option) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-PAINT-PARAM-ERROR", "expecting a QStyleOptionViewItem object as second argument to QAbstractItemDelegate::paint()");
      return 0;
   }
   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
   p = get_param(params, 2);
   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
   if (!index) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-PAINT-PARAM-ERROR", "expecting a QModelIndex object as third argument to QAbstractItemDelegate::paint()");
      return 0;
   }
   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
   qaid->getQAbstractItemDelegate()->paint(painter->getQPainter(), *(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index)));
   return 0;
}

//virtual void setEditorData ( QWidget * editor, const QModelIndex & index ) const
static QoreNode *QABSTRACTITEMDELEGATE_setEditorData(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreAbstractQWidget *editor = (p && p->type == NT_OBJECT) ? (QoreAbstractQWidget *)p->val.object->getReferencedPrivateData(CID_QWIDGET, xsink) : 0;
   if (!editor) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-SETEDITORDATA-PARAM-ERROR", "expecting a QWidget object as first argument to QAbstractItemDelegate::setEditorData()");
      return 0;
   }
   ReferenceHolder<QoreAbstractQWidget> editorHolder(editor, xsink);
   p = get_param(params, 1);
   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
   if (!index) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-SETEDITORDATA-PARAM-ERROR", "expecting a QModelIndex object as second argument to QAbstractItemDelegate::setEditorData()");
      return 0;
   }
   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
   qaid->getQAbstractItemDelegate()->setEditorData(editor->getQWidget(), *(static_cast<QModelIndex *>(index)));
   return 0;
}

////virtual void setModelData ( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const
//static QoreNode *QABSTRACTITEMDELEGATE_setModelData(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
//{
//   QoreNode *p = get_param(params, 0);
//   QoreAbstractQWidget *editor = (p && p->type == NT_OBJECT) ? (QoreAbstractQWidget *)p->val.object->getReferencedPrivateData(CID_QWIDGET, xsink) : 0;
//   if (!editor) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-SETMODELDATA-PARAM-ERROR", "expecting a QWidget object as first argument to QAbstractItemDelegate::setModelData()");
//      return 0;
//   }
//   ReferenceHolder<QoreAbstractQWidget> editorHolder(editor, xsink);
//   p = get_param(params, 1);
//   ??? QAbstractItemModel* model = p;
//   p = get_param(params, 2);
//   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
//   if (!index) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-SETMODELDATA-PARAM-ERROR", "expecting a QModelIndex object as third argument to QAbstractItemDelegate::setModelData()");
//      return 0;
//   }
//   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
//   qaid->getQAbstractItemDelegate()->setModelData(editor->getQWidget(), model, *(static_cast<QModelIndex *>(index)));
//   return 0;
//}

//virtual QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const = 0
static QoreNode *QABSTRACTITEMDELEGATE_sizeHint(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
   if (!option) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-SIZEHINT-PARAM-ERROR", "expecting a QStyleOptionViewItem object as first argument to QAbstractItemDelegate::sizeHint()");
      return 0;
   }
   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
   p = get_param(params, 1);
   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
   if (!index) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-SIZEHINT-PARAM-ERROR", "expecting a QModelIndex object as second argument to QAbstractItemDelegate::sizeHint()");
      return 0;
   }
   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
   Object *o_qs = new Object(QC_QSize, getProgram());
   QoreQSize *q_qs = new QoreQSize(qaid->getQAbstractItemDelegate()->sizeHint(*(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index))));
   o_qs->setPrivate(CID_QSIZE, q_qs);
   return new QoreNode(o_qs);
}

//virtual void updateEditorGeometry ( QWidget * editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const
static QoreNode *QABSTRACTITEMDELEGATE_updateEditorGeometry(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QoreAbstractQWidget *editor = (p && p->type == NT_OBJECT) ? (QoreAbstractQWidget *)p->val.object->getReferencedPrivateData(CID_QWIDGET, xsink) : 0;
   if (!editor) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-UPDATEEDITORGEOMETRY-PARAM-ERROR", "expecting a QWidget object as first argument to QAbstractItemDelegate::updateEditorGeometry()");
      return 0;
   }
   ReferenceHolder<QoreAbstractQWidget> editorHolder(editor, xsink);
   p = get_param(params, 1);
   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
   if (!option) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-UPDATEEDITORGEOMETRY-PARAM-ERROR", "expecting a QStyleOptionViewItem object as second argument to QAbstractItemDelegate::updateEditorGeometry()");
      return 0;
   }
   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
   p = get_param(params, 2);
   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
   if (!index) {
      if (!xsink->isException())
         xsink->raiseException("QABSTRACTITEMDELEGATE-UPDATEEDITORGEOMETRY-PARAM-ERROR", "expecting a QModelIndex object as third argument to QAbstractItemDelegate::updateEditorGeometry()");
      return 0;
   }
   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
   qaid->getQAbstractItemDelegate()->updateEditorGeometry(editor->getQWidget(), *(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index)));
   return 0;
}

////bool helpEvent ( QHelpEvent * event, QAbstractItemView * view, const QStyleOptionViewItem & option, const QModelIndex & index )
//static QoreNode *QABSTRACTITEMDELEGATE_helpEvent(Object *self, QoreAbstractQAbstractItemDelegate *qaid, QoreNode *params, ExceptionSink *xsink)
//{
//   QoreNode *p = get_param(params, 0);
//   ??? QHelpEvent* event = p;
//   p = get_param(params, 1);
//   ??? QAbstractItemView* view = p;
//   p = get_param(params, 2);
//   QoreQStyleOptionViewItem *option = (p && p->type == NT_OBJECT) ? (QoreQStyleOptionViewItem *)p->val.object->getReferencedPrivateData(CID_QSTYLEOPTIONVIEWITEM, xsink) : 0;
//   if (!option) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-HELPEVENT-PARAM-ERROR", "expecting a QStyleOptionViewItem object as third argument to QAbstractItemDelegate::helpEvent()");
//      return 0;
//   }
//   ReferenceHolder<QoreQStyleOptionViewItem> optionHolder(option, xsink);
//   p = get_param(params, 3);
//   QoreQModelIndex *index = (p && p->type == NT_OBJECT) ? (QoreQModelIndex *)p->val.object->getReferencedPrivateData(CID_QMODELINDEX, xsink) : 0;
//   if (!index) {
//      if (!xsink->isException())
//         xsink->raiseException("QABSTRACTITEMDELEGATE-HELPEVENT-PARAM-ERROR", "expecting a QModelIndex object as fourth argument to QAbstractItemDelegate::helpEvent()");
//      return 0;
//   }
//   ReferenceHolder<QoreQModelIndex> indexHolder(index, xsink);
//   return new QoreNode(qaid->getQAbstractItemDelegate()->helpEvent(event, view, *(static_cast<QStyleOptionViewItem *>(option)), *(static_cast<QModelIndex *>(index))));
//}

QoreClass *initQAbstractItemDelegateClass(QoreClass *qobject)
{
   QC_QAbstractItemDelegate = new QoreClass("QAbstractItemDelegate", QDOM_GUI);
   CID_QABSTRACTITEMDELEGATE = QC_QAbstractItemDelegate->getID();

   QC_QAbstractItemDelegate->addBuiltinVirtualBaseClass(qobject);

   QC_QAbstractItemDelegate->setConstructor(QABSTRACTITEMDELEGATE_constructor);
   QC_QAbstractItemDelegate->setCopy((q_copy_t)QABSTRACTITEMDELEGATE_copy);

   QC_QAbstractItemDelegate->addMethod("createEditor",                (q_method_t)QABSTRACTITEMDELEGATE_createEditor);
   //QC_QAbstractItemDelegate->addMethod("editorEvent",                 (q_method_t)QABSTRACTITEMDELEGATE_editorEvent);
   QC_QAbstractItemDelegate->addMethod("paint",                       (q_method_t)QABSTRACTITEMDELEGATE_paint);
   QC_QAbstractItemDelegate->addMethod("setEditorData",               (q_method_t)QABSTRACTITEMDELEGATE_setEditorData);
   //QC_QAbstractItemDelegate->addMethod("setModelData",                (q_method_t)QABSTRACTITEMDELEGATE_setModelData);
   QC_QAbstractItemDelegate->addMethod("sizeHint",                    (q_method_t)QABSTRACTITEMDELEGATE_sizeHint);
   QC_QAbstractItemDelegate->addMethod("updateEditorGeometry",        (q_method_t)QABSTRACTITEMDELEGATE_updateEditorGeometry);
   //QC_QAbstractItemDelegate->addMethod("helpEvent",                   (q_method_t)QABSTRACTITEMDELEGATE_helpEvent);

   return QC_QAbstractItemDelegate;
}

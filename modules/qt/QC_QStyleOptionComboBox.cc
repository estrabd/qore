/*
 QC_QStyleOptionComboBox.cc
 
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

#include "QC_QStyleOptionComboBox.h"

qore_classid_t CID_QSTYLEOPTIONCOMBOBOX;
class QoreClass *QC_QStyleOptionComboBox = 0;

//QStyleOptionComboBox ()
//QStyleOptionComboBox ( const QStyleOptionComboBox & other )
static void QSTYLEOPTIONCOMBOBOX_constructor(QoreObject *self, const QoreListNode *params, ExceptionSink *xsink)
{
   self->setPrivate(CID_QSTYLEOPTIONCOMBOBOX, new QoreQStyleOptionComboBox());
}

static void QSTYLEOPTIONCOMBOBOX_copy(class QoreObject *self, class QoreObject *old, class QoreQStyleOptionComboBox *qsocb, ExceptionSink *xsink)
{
   self->setPrivate(CID_QSTYLEOPTIONCOMBOBOX, new QoreQStyleOptionComboBox(*qsocb));
}

QoreClass *initQStyleOptionComboBoxClass(QoreClass *qstyleoptioncomplex)
{
   QC_QStyleOptionComboBox = new QoreClass("QStyleOptionComboBox", QDOM_GUI);
   CID_QSTYLEOPTIONCOMBOBOX = QC_QStyleOptionComboBox->getID();

   QC_QStyleOptionComboBox->addBuiltinVirtualBaseClass(qstyleoptioncomplex);

   QC_QStyleOptionComboBox->setConstructor(QSTYLEOPTIONCOMBOBOX_constructor);
   QC_QStyleOptionComboBox->setCopy((q_copy_t)QSTYLEOPTIONCOMBOBOX_copy);


   return QC_QStyleOptionComboBox;
}

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QC_AutoReadLock.h
 
 Qore Programming Language
 
 Copyright 2003 - 2012 David Nichols
 
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

#ifndef _QORE_QC_AUTOREADLOCK_H

#define _QORE_QC_AUTOREADLOCK_H

#include <qore/Qore.h>
#include <qore/intern/QC_RWLock.h>

DLLEXPORT extern qore_classid_t CID_AUTOREADLOCK;
DLLLOCAL extern QoreClass* QC_AUTOREADLOCK;

DLLLOCAL QoreClass *initAutoReadLockClass(QoreNamespace& ns);

class QoreAutoReadLock : public AbstractPrivateData {
   RWLock *rwl;

public:
   DLLLOCAL QoreAutoReadLock(RWLock *n_rwl, ExceptionSink *xsink) {
      rwl = n_rwl;
      rwl->readLock(xsink);
   }

   using AbstractPrivateData::deref;
   DLLLOCAL virtual void deref(ExceptionSink *xsink) {
      if (ROdereference()) {
	 rwl->deref(xsink);
	 delete this;
      }
   }

   DLLLOCAL virtual void destructor(ExceptionSink *xsink) {
      rwl->readUnlock(xsink);
   }
};

#endif

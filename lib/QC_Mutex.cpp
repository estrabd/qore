/*
  QC_Mutex.cpp

  Qore Programming Language
  
  Copyright 2003 - 2010 David Nichols

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
#include <qore/intern/QC_Mutex.h>

qore_classid_t CID_MUTEX;

static void MUTEX_constructor(QoreObject *self, const QoreListNode *params, ExceptionSink *xsink) {
   self->setPrivate(CID_MUTEX, new SmartMutex);
}

static void MUTEX_destructor(QoreObject *self, SmartMutex *m, ExceptionSink *xsink) {
   m->destructor(xsink);
   m->deref(xsink);
}

static void MUTEX_copy(QoreObject *self, QoreObject *old, SmartMutex *m, ExceptionSink *xsink) {
   self->setPrivate(CID_MUTEX, new SmartMutex);
}

static AbstractQoreNode *MUTEX_lock(QoreObject *self, SmartMutex *m, const QoreListNode *params, ExceptionSink *xsink) {
   m->grab(xsink);
   return 0;
}

// Mutex::lock(timeout $timeout) returns int
static AbstractQoreNode *MUTEX_lock_to(QoreObject *self, SmartMutex *m, const QoreListNode *params, ExceptionSink *xsink) {
   int timeout_ms = (int)HARD_QORE_INT(params, 0);
   int rc = m->grab(xsink, timeout_ms);
   return !*xsink ? new QoreBigIntNode(rc) : 0;
}

static AbstractQoreNode *MUTEX_trylock(QoreObject *self, SmartMutex *m, const QoreListNode *params, ExceptionSink *xsink) {
   return new QoreBigIntNode(m->tryGrab()); 
}

static AbstractQoreNode *MUTEX_unlock(QoreObject *self, SmartMutex *m, const QoreListNode *params, ExceptionSink *xsink) {
   m->release(xsink);
   return 0;
}

QoreClass *initMutexClass(QoreClass *AbstractSmartLock) {
   QORE_TRACE("initMutexClass()");

   QoreClass *QC_MUTEX = new QoreClass("Mutex", QDOM_THREAD_CLASS);
   CID_MUTEX = QC_MUTEX->getID();

   QC_MUTEX->addBuiltinVirtualBaseClass(AbstractSmartLock);

   QC_MUTEX->setConstructorExtended(MUTEX_constructor);

   QC_MUTEX->setDestructor((q_destructor_t)MUTEX_destructor);
   QC_MUTEX->setCopy((q_copy_t)MUTEX_copy);

   QC_MUTEX->addMethodExtended("lock",     (q_method_t)MUTEX_lock, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // Mutex::lock(timeout $timeout) returns int
   QC_MUTEX->addMethodExtended("lock",     (q_method_t)MUTEX_lock_to, false, QC_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 1, timeoutTypeInfo, QORE_PARAM_NO_ARG);

   QC_MUTEX->addMethodExtended("trylock",  (q_method_t)MUTEX_trylock, false, QC_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
   QC_MUTEX->addMethodExtended("unlock",   (q_method_t)MUTEX_unlock, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   return QC_MUTEX;
}

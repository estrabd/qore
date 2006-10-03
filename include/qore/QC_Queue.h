/* 
   QC_Queue.h

   Qore Programming Language

   Copyright (C) 2003, 2004, 2005, 2006 David Nichols

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

#ifndef _QORE_CLASS_QUEUE

#define _QORE_CLASS_QUEUE

#include <qore/common.h>
#include <qore/qore_thread.h>
#include <qore/support.h>
#include <qore/QoreQueue.h>
#include <qore/ReferenceObject.h>
#include <qore/Exception.h>

extern int CID_QUEUE;
class QoreClass *initQueueClass();

class Queue : public ReferenceObject, public QoreQueue
{
   protected:
      inline ~Queue() {}

   public:
      inline Queue() {}
      inline Queue(QoreNode *n) : QoreQueue(n) {}
      inline void deref(class ExceptionSink *xsink);
};

inline void Queue::deref(class ExceptionSink *xsink)
{
   if (ROdereference())
   {
      del(xsink);
      delete this;
   }
}

#endif // _QORE_CLASS_QUEUE

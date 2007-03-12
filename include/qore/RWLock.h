/* 
  RWLock.h

  Read-Write Lock object (default: prefer readers)

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

#ifndef _QORE_CLASS_RWLOCK

#define _QORE_CLASS_RWLOCK

#include <qore/AbstractSmartLock.h>
#include <qore/VLock.h>

#include <map>

// to track TIDs and read counts of readers
typedef std::map<int, int> tid_map_t;

// ASL mapping:
// asl_cond: write cond
// waiting: waiting write requests
// tid: write TID

class RWLock : public AbstractSmartLock
{
   private:
      int readRequests;
      QoreCondition read;
      bool prefer_writers;
      tid_map_t tmap;
      vlock_map_t vmap;

      // 0 = last read lock in this thread released
      DLLLOCAL int cleanup_read_lock_intern(tid_map_t::iterator i);
      DLLLOCAL void mark_read_lock_intern(int mtid, class VLock *nvl);

      DLLLOCAL virtual void cleanup();
      DLLLOCAL virtual void signalAllImpl();
      DLLLOCAL virtual void signalImpl();
      DLLLOCAL virtual int releaseImpl();
      DLLLOCAL virtual int releaseImpl(class ExceptionSink *xsink);
      DLLLOCAL virtual int grabImpl(int mtid, class VLock *nvl, class ExceptionSink *xsink, int timeout_ms = 0);
      DLLLOCAL virtual int tryGrabImpl(int mtid, class VLock *nvl);
      DLLLOCAL virtual void destructorImpl(class ExceptionSink *xsink);

   protected:

   public:
      DLLLOCAL RWLock(bool p = false);
#ifdef DEBUG
      DLLLOCAL virtual ~RWLock();
#endif
      DLLLOCAL int readLock(class ExceptionSink *xsink, int timeout_ms = 0);
      DLLLOCAL int readUnlock(class ExceptionSink *xsink);
      DLLLOCAL int tryReadLock();
      //DLLLOCAL void writeToRead(class ExceptionSink *xsink);

      DLLLOCAL int numReaders();
      DLLLOCAL int getReadWaiting() const
      {
	 return readRequests;
      }
      DLLLOCAL int getWriteWaiting() const
      {
	 return waiting;
      }

      DLLLOCAL virtual const char *getName() const { return "RWLock"; }
};

#endif // _QORE_CLASS_RWLOCK

/*
  VRMutex.cc

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
#include <qore/VRMutex.h>

#include <assert.h>

VRMutex::VRMutex() : count(0)
{
}

int VRMutex::enter(class ExceptionSink *xsink)
{
   int mtid = gettid();
   class VLock *nvl = getVLock();
   AutoLocker al(&asl_lock);
   int rc = grabImpl(mtid, nvl, xsink);
   if (!rc)
      grab_intern_intern(mtid, nvl);
   return rc;
}

int VRMutex::exit()
{
   AutoLocker al(&asl_lock);
   int rc = releaseImpl();
   if (!rc)
      release_intern_intern();
   return rc;
}

int VRMutex::grabImpl(int mtid, class VLock *nvl, class ExceptionSink *xsink)
{
   while (tid != -1 && tid != mtid)
   {
      if (tid == -2)
      {
	 xsink->raiseException("VRMUTEX-ERROR", "%s has been deleted in another thread", getName());
	 return -1;
      }

      ++waiting;
      int rc = nvl->waitOn((AbstractSmartLock *)this, vl, mtid, xsink);
      --waiting;
      // if rc is non-zero there was a deadlock
      if (rc)
	 return -1;
   }
   // the thread lock list must always be the same if the lock was grabbed
   assert((mtid == tid  && vl == nvl) || (tid == -1 && !vl));

   printd(5, "VRMutex::enter() this=%p count: %d->%d\n", this, count, count + 1);

   return count++;
}

int VRMutex::grabImpl(int mtid, int timeout_ms, class VLock *nvl, class ExceptionSink *xsink)
{
   while (tid != -1 && tid != mtid)
   {
      if (tid == -2)
      {
	 xsink->raiseException("LOCK-ERROR", "%s has been deleted in another thread", getName());
	 return -1;
      }

      ++waiting;
      int rc = nvl->waitOn((AbstractSmartLock *)this, vl, mtid, timeout_ms, xsink);
      --waiting;
      // if rc is non-zero there was a timeout or deadlock
      if (rc)
	 return -1;
   }
   // the thread lock list must always be the same if the lock was grabbed
   assert((mtid == tid  && vl == nvl) || (tid == -1 && !vl));

   printd(5, "VRMutex::enter() this=%p count: %d->%d\n", this, count, count + 1);

   return count++;
}

int VRMutex::tryGrabImpl(int mtid, class VLock *nvl)
{
   if (tid != -1 && tid != mtid)
      return -1;

   // the thread lock list must always be the same if the lock was grabbed
   assert((mtid == tid  && vl == nvl) || (tid == -1 && !vl));

   printd(5, "VRMutex::enter() this=%p count: %d->%d\n", this, count, count + 1);

   return count++;
}

// internal use only
int VRMutex::releaseImpl()
{
   assert(tid == gettid());
   printd(5, "VRMutex::exit() this=%p count: %d->%d\n", this, count, count - 1);

   --count;
   // if this is the last thread from the group to exit the lock, then return 0
   return count ? -1 : 0;
}

int VRMutex::releaseImpl(class ExceptionSink *xsink)
{
   int mtid = gettid();
   if (tid == -1)
   {
      // use getName() here so it can be safely inherited
      xsink->raiseException("LOCK-ERROR", "%s::exit() called without acquiring the lock", getName());
      return -1;
   }
   else if (tid != mtid)
   {
      // use getName() here so it can be safely inherited
      xsink->raiseException("LOCK-ERROR", "%s::exit() called by TID %d while the lock is held by TID %d", getName(), tid, mtid);
      return -1;      
   }
   // count must be > 0 because tid != 0
   assert(count);

   printd(5, "VRMutex::exit() this=%p count: %d->%d\n", this, count, count - 1);

   --count;
   // if this is the last thread from the group to exit the lock, then return 0
   return count ? -1 : 0;
}

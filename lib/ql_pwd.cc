/*
  ql_pwd.cc
  
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

#include <qore/config.h>
#include <qore/ql_env.h>
#include <qore/QoreNode.h>
#include <qore/support.h>
#include <qore/Object.h>
#include <qore/params.h>
#include <qore/qore_thread.h>
#include <qore/LockedObject.h>
#include <qore/BuiltinFunctionList.h>

#include <stdio.h>
#include <pwd.h>

// for the getpwuid function
static class LockedObject lck_getpwuid;

static inline void assign_value(Hash *h, char *key, char *val)
{
   h->setKeyValue(key, new QoreNode(val), NULL);
}

static inline void assign_value(Hash *h, char *key, int val)
{
   h->setKeyValue(key, new QoreNode(NT_INT, val), NULL);
}

static class QoreNode *f_getpwuid(class QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p0, *rv;

   tracein("f_getpwuid()");
   if (!(p0 = get_param(params, 0)))
   {
      traceout("f_getpwuid()");      
      return NULL;
   }

   lck_getpwuid.lock();
   
   struct passwd *pw = getpwuid(p0->getAsInt());
   if (pw)
   {
      Hash *h = new Hash();
      // assign values
      assign_value(h, "pw_name", pw->pw_name);
      assign_value(h, "pw_passwd", pw->pw_passwd);
      assign_value(h, "pw_gecos", pw->pw_gecos);
      assign_value(h, "pw_dir", pw->pw_dir);
      assign_value(h, "pw_shell", pw->pw_shell);
      assign_value(h, "pw_uid", pw->pw_uid);
      assign_value(h, "pw_gid", pw->pw_gid);
      rv = new QoreNode(h);
   }
   else
      rv = NULL;

   lck_getpwuid.unlock();

   traceout("f_getpwuid()");
   return rv;
}

void init_pwd_functions()
{
   builtinFunctions.add("getpwuid", f_getpwuid);
}

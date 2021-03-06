/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ExceptionSink.h

  Qore Programming Language ExceptionSink class definition

  Copyright (C) 2003 - 2015 David Nichols

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QORE_EXCEPTIONSINK_H

#define _QORE_EXCEPTIONSINK_H

#include <stdarg.h>
#include <stdio.h>

class QoreException;
struct QoreProgramLocation;

//! container for holding Qore-language exception information and also for registering a "thread_exit" call
class ExceptionSink {
   friend struct qore_es_private;

private:
   //! private implementation of the class
   struct qore_es_private *priv;

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL ExceptionSink(const ExceptionSink&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL ExceptionSink& operator=(const ExceptionSink&);
      
public:
   //! creates an empty ExceptionSink object
   DLLEXPORT ExceptionSink();

   //! calls ExceptionSink::defaultExceptionHandler() on all exceptions still present in the object and then deletes the exception list
   DLLEXPORT ~ExceptionSink();

   //! calls ExceptionSink::defaultExceptionHandler() on all exceptions still present in the object and then deletes the exception list
   DLLEXPORT void handleExceptions();

   //! calls ExceptionSink::defaultWarningHandler() on all exceptions still present in the object and then deletes the exception list
   DLLEXPORT void handleWarnings();

   //! returns true if at least one exception is present or thread_exit has been triggered
   DLLEXPORT bool isEvent() const;

   //! returns true if thread_exit has been triggered
   DLLEXPORT bool isThreadExit() const;

   //! returns true if at least one exception is present
   DLLEXPORT bool isException() const;

   //! returns true if at least one exception is present or thread_exit has been triggered
   /** Intended as a alternative to isEvent()
       @code
       ExceptionSink xsink;
       if (xsink) { .. }
       @endcode
   */
   DLLEXPORT operator bool () const;

   //! appends a Qore-language exception to the list
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param fmt the format string for the description for the exception
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode *raiseException(const char *err, const char *fmt, ...);

   //! appends a Qore-language exception to the list and appends the result of strerror(errno) to the description
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param en the error number (normally "errno")
       @param fmt the format string for the description for the exception
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode* raiseErrnoException(const char *err, int en, const char *fmt, ...);

   //! appends a Qore-language exception to the list and appends the result of strerror(errno) to the description
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param en the error number (normally "errno")
       @param desc the error description (the ExceptionSink object takes over ownership of the reference count)
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode* raiseErrnoException(const char *err, int en, QoreStringNode* desc);

   //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference count of 'arg')
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
       @param fmt the format string for the description for the exception
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode *raiseExceptionArg(const char* err, AbstractQoreNode* arg, const char* fmt, ...);

   //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference counts of 'arg' and 'desc')
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
       @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode *raiseExceptionArg(const char* err, AbstractQoreNode* arg, QoreStringNode *desc);

   //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
   /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
       @param err the exception code string
       @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
       @return always returns 0
   */
   DLLEXPORT AbstractQoreNode *raiseException(const char *err, QoreStringNode *desc);

   //! sets the "thread_exit" flag; will cause the current thread to terminate
   DLLEXPORT void raiseThreadExit();

   //! assimilates all entries of the "xs" argument by appending them to the internal list and deletes the "xs" argument
   DLLEXPORT void assimilate(ExceptionSink *xs);

   //! assimilates all entries of the "xs" argument by appending them to the internal list and clears the "xs" argument
   DLLEXPORT void assimilate(ExceptionSink &xs);

   //! intended to be used to handle out of memory errors FIXME: not yet fully implemented
   DLLEXPORT void outOfMemory();

   //! deletes the exception list immediately
   DLLEXPORT void clear();

   DLLLOCAL void raiseException(QoreException* e);
   DLLLOCAL void raiseException(const QoreListNode* n);
   DLLLOCAL void raiseException(const QoreProgramLocation& loc, const char* err, AbstractQoreNode* arg, AbstractQoreNode* desc);
   DLLLOCAL void raiseException(const QoreProgramLocation& loc, const char* err, AbstractQoreNode* arg, const char* fmt, ...);
   DLLLOCAL QoreException* catchException();
   DLLLOCAL void overrideLocation(const QoreProgramLocation& loc);
   DLLLOCAL void rethrow(QoreException* old);

   DLLLOCAL static void defaultExceptionHandler(QoreException* e);
   DLLLOCAL static void defaultWarningHandler(QoreException* e);
};

static inline void alreadyDeleted(ExceptionSink *xsink, const char *cmeth) {
   xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s() cannot be executed because the object has already been deleted", cmeth);
}

static inline void makeAccessDeletedObjectException(ExceptionSink *xsink, const char *mem, const char *cname) {
   xsink->raiseException("OBJECT-ALREADY-DELETED", "attempt to access member '%s' of an already-deleted object of class '%s'", mem, cname);
}

static inline void makeAccessDeletedObjectException(ExceptionSink *xsink, const char *cname) {
   xsink->raiseException("OBJECT-ALREADY-DELETED", "attempt to access an already-deleted object of class '%s'", cname);
}

#endif

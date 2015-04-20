/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  RangeIterator.h

  Qore Programming Language

  Copyright (C) 2003 - 2015 Qore Technologies s r.o.

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

#ifndef _QORE_RANGEITERATOR_H

#define _QORE_RANGEITERATOR_H

// the c++ object. See QC_RangeIterator.qpp for docs.
class RangeIterator : public QoreIteratorBase {
private:
   int64 m_start;
   int64 m_stop;
   int64 m_step;

   int64 m_position;

   bool m_increasing;
   bool m_valid;

   AbstractQoreNode* val;

public:
   DLLLOCAL RangeIterator(int64 start, int64 stop, int64 step, const AbstractQoreNode* v, ExceptionSink* xsink)
      : QoreIteratorBase(),
        m_start(start),
        m_stop(stop),
        m_step(step),
        m_position(-1),
        m_increasing(start<stop),
        m_valid(false),
        val((v && step >= 0) ? v->refSelf() : 0) {
      if (step < 1) {
         xsink->raiseException("RANGEITERATOR-ERROR", "Value of the 'step' argument has to be greater than 0 (value passed: %d)", step);
      }
   }

   DLLLOCAL RangeIterator(const RangeIterator& old)
      : m_start(old.m_start), m_stop(old.m_stop), m_step(old.m_step),
        m_position(old.m_position), m_increasing(old.m_increasing),
        m_valid(old.m_valid), val(old.val ? old.val->refSelf() : 0) {
   }

   DLLLOCAL virtual ~RangeIterator() {
      assert(!val);
   }

   DLLLOCAL void destructor(ExceptionSink* xsink) {
      if (val) {
         val->deref(xsink);
#ifdef DEBUG
         val = 0;
#endif
      }
   }

   DLLLOCAL bool next() {
      ++m_position;
      m_valid = m_increasing ? (calculateCurrent() <= m_stop) : (calculateCurrent() >= m_stop);
      if (!m_valid)
         m_position = -1;
      return m_valid;
   }

   DLLLOCAL bool valid() const {
      return m_valid;
   }

   DLLLOCAL AbstractQoreNode* getValue(ExceptionSink *xsink) {
      if (!m_valid) {
         xsink->raiseException("INVALID-ITERATOR", "the %s is not pointing at a valid element; make sure %s::next() returns True before calling this method", getName(), getName());
         return 0;
      }
      
      int64 rv = calculateCurrent();
      return val ? val->refSelf() : new QoreBigIntNode(rv);
   }

   DLLLOCAL void reset() {
      m_position = -1;
      m_valid = false;
   }

   DLLLOCAL virtual const char* getName() const { return "RangeIterator"; }

private:
   DLLLOCAL int64 calculateCurrent() {
      if (m_increasing) {
         return m_start + (m_position * m_step);
      }
      else {
         return m_start - (m_position * m_step);
      }
   }
};

#endif // _QORE_RANGEITERATOR_H

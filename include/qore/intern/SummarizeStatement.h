/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 SummarizeStatement.h
 
 Qore Programming Language
 
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

#ifndef _QORE_SUMMARIZESTATEMENT_H

#define _QORE_SUMMARIZESTATEMENT_H

#include "ContextStatement.h"

class SummarizeStatement : public ContextStatement {
   DLLLOCAL virtual int execImpl(class AbstractQoreNode **return_value, class ExceptionSink *xsink);
   DLLLOCAL virtual int parseInitImpl(LocalVar *oflag, int pflag = 0);

public:
   class AbstractQoreNode *summarize;
      
   DLLLOCAL SummarizeStatement(int start_line, int end_line, char *n, class AbstractQoreNode *expr, class ContextModList *cm, class StatementBlock *cd, class AbstractQoreNode *summ_exp = NULL) : ContextStatement(start_line, end_line, n, expr, cm, cd), summarize(summ_exp) {
   }
   DLLLOCAL virtual ~SummarizeStatement() {
      if (summarize)
         summarize->deref(NULL);
   }
};

#endif

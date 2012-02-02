/*
 QoreIntShiftRightEqualsOperatorNode.cpp
 
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

#include <qore/Qore.h>

AbstractQoreNode *QoreIntShiftRightEqualsOperatorNode::evalImpl(ExceptionSink *xsink) const {
   int64 rv = QoreIntShiftRightEqualsOperatorNode::bigIntEvalImpl(xsink);
   if (!ref_rv || *xsink)
      return 0;

   return new QoreBigIntNode(rv);
}

AbstractQoreNode *QoreIntShiftRightEqualsOperatorNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = ref_rv;
   return QoreIntShiftRightEqualsOperatorNode::evalImpl(xsink);
}

int64 QoreIntShiftRightEqualsOperatorNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   int64 rv = right->bigIntEval(xsink);
   if (*xsink)
      return 0;

   VarRefNode *v = reinterpret_cast<VarRefNode *>(left);
   return v->shiftRightEqualsBigInt(rv, xsink);
}

int QoreIntShiftRightEqualsOperatorNode::integerEvalImpl(ExceptionSink *xsink) const {
   return QoreIntShiftRightEqualsOperatorNode::bigIntEvalImpl(xsink);
}

double QoreIntShiftRightEqualsOperatorNode::floatEvalImpl(ExceptionSink *xsink) const {
   return QoreIntShiftRightEqualsOperatorNode::bigIntEvalImpl(xsink);
}

bool QoreIntShiftRightEqualsOperatorNode::boolEvalImpl(ExceptionSink *xsink) const {
   return QoreIntShiftRightEqualsOperatorNode::bigIntEvalImpl(xsink);
}

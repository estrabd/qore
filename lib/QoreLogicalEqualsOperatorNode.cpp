/*
  QoreLogicalEqualsOperatorNode.cpp
 
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

#include <qore/Qore.h>

QoreString QoreLogicalEqualsOperatorNode::logical_equals_str("logical equals operator expression");
QoreString QoreLogicalNotEqualsOperatorNode::logical_not_equals_str("logical not equals operator expression");

bool QoreLogicalEqualsOperatorNode::boolEvalImpl(ExceptionSink *xsink) const {
   if (pfunc)
      return (this->*pfunc)(xsink);

   QoreNodeEvalOptionalRefHolder l(left, xsink);
   if (*xsink)
      return false;
   QoreNodeEvalOptionalRefHolder r(right, xsink);
   if (*xsink)
      return false;

   return softEqual(*l, *r, xsink);
}

AbstractQoreNode *QoreLogicalEqualsOperatorNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = boolTypeInfo;

   const QoreTypeInfo *lti = 0, *rti = 0;

   left = left->parseInit(oflag, pflag, lvids, lti);
   right = right->parseInit(oflag, pflag, lvids, rti);

   // see if both arguments are constants, then eval immediately and substitute this node with the result
   if (left && left->is_value() && right && right->is_value()) {
      SimpleRefHolder<QoreLogicalEqualsOperatorNode> del(this);
      ParseExceptionSink xsink;
      AbstractQoreNode *rv = get_bool_node(softEqual(left, right, *xsink));
      return rv;
   }

   // check for optimizations based on type, but only assign if neither side is a string (highest priority)
   // and types are known for both operands (if not, QoreTypeInfo::parseReturnsType(NT_STRING) will return QTI_AMBIGUOUS
   if (!lti->parseReturnsType(NT_STRING) && !rti->parseReturnsType(NT_STRING)) {
      if (lti->isType(NT_FLOAT) || rti->isType(NT_FLOAT))
	 pfunc = &QoreLogicalEqualsOperatorNode::floatSoftEqual;
      else if (lti->isType(NT_INT) || rti->isType(NT_INT))
	 pfunc = &QoreLogicalEqualsOperatorNode::bigIntSoftEqual;
      else if (lti->isType(NT_BOOLEAN) || rti->isType(NT_BOOLEAN))
	 pfunc = &QoreLogicalEqualsOperatorNode::boolSoftEqual;
   }

   return this;
}

bool QoreLogicalEqualsOperatorNode::softEqual(const AbstractQoreNode *left, const AbstractQoreNode *right, ExceptionSink *xsink) {
   if (!left)
      left = &Nothing;
   if (!right)
      right = &Nothing;

   qore_type_t lt = left->getType();
   qore_type_t rt = right->getType();

   if (lt == NT_STRING) {
      const QoreStringNode* l = reinterpret_cast<const QoreStringNode*>(left);
      if (rt == NT_STRING)
	 return l->equalSoft(*reinterpret_cast<const QoreStringNode*>(right), xsink);
      QoreStringValueHelper r(right, l->getEncoding(), xsink);
      if (*xsink)
	 return false;
      return l->equal(*r);
   }

   if (rt == NT_STRING) {
      const QoreStringNode* r = reinterpret_cast<const QoreStringNode*>(right);
      QoreStringValueHelper l(left, r->getEncoding(), xsink);
      if (*xsink)
	 return false;
      return l->equal(*r);
   }

   if (lt == NT_NUMBER) {
      switch (rt) {
	 case NT_NUMBER:
	    return reinterpret_cast<const QoreNumberNode*>(left)->compare(*reinterpret_cast<const QoreNumberNode*>(right)) == 0;
	 case NT_FLOAT:
	    return reinterpret_cast<const QoreNumberNode*>(left)->compare(reinterpret_cast<const QoreFloatNode*>(right)->f) == 0;
	 case NT_INT:
	    return reinterpret_cast<const QoreNumberNode*>(left)->compare(reinterpret_cast<const QoreBigIntNode*>(right)->val) == 0;
	 default: {
	    ReferenceHolder<QoreNumberNode> rn(new QoreNumberNode(right), xsink);
	    return reinterpret_cast<const QoreNumberNode*>(left)->compare(**rn) == 0;
	 }
      }
   }

   if (rt == NT_NUMBER) {
      assert(lt != NT_NUMBER);
      switch (lt) {
	 case NT_FLOAT:
	    return reinterpret_cast<const QoreNumberNode*>(right)->compare(reinterpret_cast<const QoreFloatNode*>(left)->f) == 0;
	 case NT_INT:
	    return reinterpret_cast<const QoreNumberNode*>(right)->compare(reinterpret_cast<const QoreBigIntNode*>(left)->val) == 0;
	 default: {
	    ReferenceHolder<QoreNumberNode> ln(new QoreNumberNode(left), xsink);
	    return reinterpret_cast<const QoreNumberNode*>(right)->compare(**ln) == 0;
	 }
      }
   } 

   if (lt == NT_FLOAT || rt == NT_FLOAT)
      return left->getAsFloat() == right->getAsFloat();

   if (lt == NT_INT || rt == NT_INT)
      return left->getAsBigInt() == right->getAsBigInt();

   if (lt == NT_BOOLEAN || rt == NT_BOOLEAN)
      return left->getAsBigInt() == right->getAsBool();

   if (lt == NT_DATE || rt == NT_DATE) {
      DateTimeNodeValueHelper l(left);
      DateTimeNodeValueHelper r(right);
      return l->isEqual(*r);
   }

   return left->is_equal_soft(right, xsink);
}

bool QoreLogicalEqualsOperatorNode::floatSoftEqual(ExceptionSink *xsink) const {
   double l = left->floatEval(xsink);
   if (*xsink) return false;
   double r = right->floatEval(xsink);
   if (*xsink) return false;

   return l == r;
}

bool QoreLogicalEqualsOperatorNode::bigIntSoftEqual(ExceptionSink *xsink) const {
   int64 l = left->bigIntEval(xsink);
   if (*xsink) return false;
   int64 r = right->bigIntEval(xsink);
   if (*xsink) return false;

   return l == r;
}

bool QoreLogicalEqualsOperatorNode::boolSoftEqual(ExceptionSink *xsink) const {
   bool l = left->boolEval(xsink);
   if (*xsink) return false;
   bool r = right->boolEval(xsink);
   if (*xsink) return false;

   return l == r;
}

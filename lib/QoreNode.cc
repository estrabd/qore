/*
  QoreNode.cc

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
#include <qore/intern/Find.h>

// TEMP!! FIXME: DELETEME:!
#include <qore/intern/QT_backquote.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define TRACK_REFS 1

#if TRACK_REFS

#endif

QoreNode::QoreNode(const QoreType *t) 
{
   type = t; 
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(double f)
{
   type = NT_FLOAT;
   val.floatval = f;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::~QoreNode()
{
#if 0
   printd(5, "QoreNode::~QoreNode() type=%s\n", getTypeName());
#endif

   if (type == NT_COMPLEXCONTEXTREF) {
      delete val.complex_cref;
      return;
   }

   if (type == NT_VARREF) {
      delete val.vref;
      return;
   }

   if (type == NT_BACKQUOTE) {
      if (val.c_str)
	 free(val.c_str);
      return;
   }

   if (type == NT_SELF_VARREF) {
      if (val.c_str)
	 free(val.c_str);
      return;
   }

   if (type == NT_BAREWORD) {
      if (val.c_str)
	 free(val.c_str);
      return;
   }

   if (type == NT_CONTEXTREF) {
      if (val.c_str)
	 free(val.c_str);
      return;
   }

   if (type == NT_TREE) {
      delete val.tree;
      return;
   }

   if (type == NT_FIND) {
      delete val.find;
      return;
   }

   if (type == NT_FUNCTION_CALL) {
      delete val.fcall;
      return;
   }

   if (type == NT_SCOPE_REF) {
      delete val.socall;
      return;
   }

   if (type == NT_CONSTANT) {
      delete val.scoped_ref;
      return;
   }

   if (type == NT_CLASSREF) {
      delete val.classref;
      return;
   }

   if (type == NT_REFERENCE) {
      val.lvexp->deref(NULL);
      return;
   }

   if (type == NT_REGEX_SUBST) {
      delete val.resub;
      return;
   }

   if (type == NT_REGEX_TRANS) {
      delete val.retrans;
      return;
   }

   if (type == NT_REGEX) {
      delete val.regex;
      return;
   }

   if (type == NT_OBJMETHREF) {
      delete val.objmethref;
      return;
   }
}

QoreNode::QoreNode(char *name, QoreListNode *a)
{
   type = NT_FUNCTION_CALL;
   val.fcall = new FunctionCall(name, a);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

// for non-scoped in-object calls
QoreNode::QoreNode(QoreListNode *a, char *name)
{
   type = NT_FUNCTION_CALL;
   val.fcall = new FunctionCall(a, name);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

// for in-object base class calls
QoreNode::QoreNode(QoreListNode *a, class NamedScope *n)
{
   type = NT_FUNCTION_CALL;
   val.fcall = new FunctionCall(a, n);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(UserFunction *u, QoreListNode *a)
{
   type = NT_FUNCTION_CALL;
   val.fcall = new FunctionCall(u, a);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(BuiltinFunction *b, QoreListNode *a)
{
   type = NT_FUNCTION_CALL;
   val.fcall = new FunctionCall(b, a);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class NamedScope *n, QoreListNode *a)
{
   type = NT_SCOPE_REF;
   val.socall = new ScopedObjectCall(n, a);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class NamedScope *n)
{
   type = NT_CONSTANT;
   val.scoped_ref = n;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class ClassRef *c)
{
   type = NT_CLASSREF;
   val.classref = c;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class VarRef *v)
{
   type = NT_VARREF;
   val.vref = v;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class QoreNode *l, class Operator *o, class QoreNode *r)
{
   type = NT_TREE;
   val.tree = new Tree(l, o, r);
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class RegexSubst *rs)
{
   type = NT_REGEX_SUBST;
   val.resub = rs;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class RegexTrans *rt)
{
   type = NT_REGEX_TRANS;
   val.retrans = rt;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class QoreRegex *r)
{
   type = NT_REGEX;
   val.regex = r;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class Tree *t)
{
   type = NT_TREE;
   val.tree = t;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class ComplexContextRef *ccref)
{
   type = NT_COMPLEXCONTEXTREF;
   val.complex_cref = ccref;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class FunctionReferenceCall *frc)
{
   type = NT_FUNCREFCALL;
   val.funcrefcall = frc;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class AbstractFunctionReference *afr)
{
   type = NT_FUNCREF;
   val.funcref = afr;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class AbstractParseObjectMethodReference *objmethref)
{
   type = NT_OBJMETHREF;
   val.objmethref = objmethref;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

QoreNode::QoreNode(class FunctionCall *fc)
{
   type = NT_FUNCTION_CALL;
   val.fcall = fc;
#if TRACK_REFS
   printd(5, "QoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

void QoreNode::ref() const
{
#ifdef DEBUG
#if TRACK_REFS
   const QoreObject *o = dynamic_cast<const QoreObject *>(this);
   if (o)
      printd(5, "QoreNode::ref() %08p type=%s (%d->%d) object=%08p, class=%s\n", this, getTypeName(), references, references + 1, o, o->getClass()->getName());
   else
      printd(5, "QoreNode::ref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references + 1);
#endif
#endif
   ROreference();
}

class QoreNode *QoreNode::RefSelf() const
{
   ref();
   return const_cast<QoreNode *>(this);
}

void QoreNode::deref(ExceptionSink *xsink)
{
   //tracein("QoreNode::deref()");
#ifdef DEBUG
#if TRACK_REFS
   if (type == NT_STRING) printd(5, "QoreNode::deref() %08p (%d->%d) string='%s'\n", this, references, references - 1, ((QoreStringNode *)this)->getBuffer());
   else
      printd(5, "QoreNode::deref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references - 1);

#endif
   if (references > 51200 || references < 0)
   {
      if (type == NT_STRING)
	 printd(0, "QoreNode::deref() WARNING, node %08p references=%d (type=%s) (val=\"%s\")\n",
		this, references, getTypeName(), ((QoreStringNode *)this)->getBuffer());
      else
	 printd(0, "QoreNode::deref() WARNING, node %08p references=%d (type=%s)\n",
		this, references, getTypeName());
      assert(false);
   }
#endif
   assert(references > 0);

   if (ROdereference())
   {
if (type == NT_FUNCREF)
	 val.funcref->del(xsink);
	 
      // now delete this QoreNode
      delete this;
   }

   //traceout("QoreNode::deref()");
}

class QoreNode *QoreNode::realCopy() const
{
   assert(this);

   if (type == NT_FLOAT) 
      return new QoreNode(val.floatval);

   if (type == NT_BACKQUOTE) {
      QoreNode *n = new QoreNode(type);
      n->val.c_str = val.c_str ? strdup(val.c_str) : 0;
      return n;
   }

   if (type == NT_CONTEXTREF) {
      QoreNode *n = new QoreNode(type);
      n->val.c_str = val.c_str ? strdup(val.c_str) : 0;
      return n;
   }

   if (type == NT_SELF_VARREF) {
      QoreNode *n = new QoreNode(type);
      n->val.c_str = val.c_str ? strdup(val.c_str) : 0;
      return n;
   }

   if (type == NT_BAREWORD) {
      QoreNode *n = new QoreNode(type);
      n->val.c_str = val.c_str ? strdup(val.c_str) : 0;
      return n;
   }

   if (type == NT_COMPLEXCONTEXTREF) {
      return new QoreNode(val.complex_cref->copy());
   }

   if (type == NT_VARREF) {
      return new QoreNode(val.vref->copy());
   }

   if (type == NT_TREE)
      assert(false);

   if (type == NT_FIND)
      assert(false);

   if (type == NT_FUNCTION_CALL)
      assert(false);

   if (type == NT_SCOPE_REF)
      assert(false);

   if (type == NT_CONSTANT)
      assert(false);

   if (type == NT_REFERENCE)
      assert(false);

   if (type == NT_REGEX_SUBST)
      assert(false);

   if (type == NT_REGEX_TRANS)
      assert(false);

   if (type == NT_REGEX)
      assert(false);

   if (type == NT_CLASSREF)
      assert(false);

   QoreNode *rv = new QoreNode(type);
   memcpy(&rv->val, &val, sizeof(union node_u));
   return rv;
}

bool QoreNode::needs_eval() const
{
   if (type == NT_BACKQUOTE)
      return true;

   if (type == NT_CONTEXTREF)
      return true;

   if (type == NT_COMPLEXCONTEXTREF)
      return true;

   if (type == NT_VARREF)
      return true;

   if (type == NT_TREE)
      return true;

   if (type == NT_FIND)
      return true;

   if (type == NT_FUNCTION_CALL)
      return true;

   if (type == NT_SELF_VARREF)
      return true;

   if (type == NT_CONTEXT_ROW)
      return true;

   if (type == NT_OBJMETHREF)
      return true;

   if (type == NT_FUNCREF)
      return true;

   if (type == NT_FUNCREFCALL)
      return true;

   return false;
}

bool QoreNode::is_value() const
{
   if (type == NT_BACKQUOTE)
      return false;

   if (type == NT_CONTEXTREF)
      return false;

   if (type == NT_COMPLEXCONTEXTREF)
      return false;

   if (type == NT_VARREF)
      return false;

   if (type == NT_TREE)
      return false;

   if (type == NT_FIND)
      return false;

   if (type == NT_FUNCTION_CALL)
      return false;

   if (type == NT_SELF_VARREF)
      return false;

   if (type == NT_CONTEXT_ROW)
      return false;

   if (type == NT_OBJMETHREF)
      return false;

   if (type == NT_FUNCREF)
      return false;

   if (type == NT_FUNCREFCALL)
      return false;

   if (type == NT_BAREWORD)
      return false;

   if (type == NT_CONSTANT)
      return false;

   if (type == NT_REFERENCE)
      return false;

   if (type == NT_REGEX)
      return false;

   if (type == NT_CLASSREF)
      return false;

   if (type == NT_OBJMETHREF)
      return false;

   if (type == NT_SCOPE_REF)
      return false;

   return true;
}

#ifndef READ_BLOCK
#define READ_BLOCK 1024
#endif

class QoreNode *backquoteEval(const char *cmd, ExceptionSink *xsink)
{
   // execute command in a new process and read stdout in parent
   FILE *p = popen(cmd, "r");
   if (!p)
   {
      // could not fork or create pipe
      xsink->raiseException("BACKQUOTE-ERROR", strerror(errno));
      return 0;
   }

   // allocate buffer for return value
   TempQoreStringNode s(new QoreStringNode());

   // read in result string
   while (1)
   {
      char buf[READ_BLOCK];
      int size = fread(buf, 1, READ_BLOCK, p);

      // break if no data is available or an error occurred
      if (!size || size == -1)
         break;

      s->concat(buf, size);

      // break if there is no more data
      if (size != READ_BLOCK)
         break;
   }

   // wait for child process to terminate and close pipe
   pclose(p);

   if (!s->strlen())
      return 0;

   return s.release();
}

/*
  QoreNode::eval(): return value requires a dereference
 */
class QoreNode *QoreNode::eval(ExceptionSink *xsink) const
{
   if (type == NT_BACKQUOTE) {
      return backquoteEval(val.c_str, xsink);
   }

   if (type == NT_CONTEXTREF) {
      return evalContextRef(val.c_str, xsink);
   }

   if (type == NT_COMPLEXCONTEXTREF) {
      return evalComplexContextRef(val.complex_cref, xsink);
   }

   if (type == NT_VARREF) {
      return val.vref->eval(xsink);
   }

   if (type == NT_TREE) {
      return val.tree->eval(xsink);
   }

   if (type == NT_FIND) {
      return val.find->eval(xsink);
   }

   if (type == NT_FUNCTION_CALL) {
      return val.fcall->eval(xsink);
   }

   if (type == NT_SELF_VARREF) {
      assert(getStackObject());
      return getStackObject()->evalMemberNoMethod(val.c_str, xsink);
   }

   if (type == NT_CONTEXT_ROW) {
      return evalContextRow(xsink);
   }

   if (type == NT_OBJMETHREF) {
      return val.objmethref->eval(xsink);
   }

   if (type == NT_FUNCREF) {
      return val.funcref->eval(this);
   }

   if (type == NT_FUNCREFCALL) {
      return val.funcrefcall->eval(xsink);
   }

   if (type == NT_SCOPE_REF)
      assert(false);

   if (type == NT_CONSTANT)
      assert(false);

   if (type == NT_BAREWORD)
      assert(false);

   if (type == NT_REGEX_SUBST)
      assert(false);

   if (type == NT_REGEX_TRANS)
      assert(false);

   return RefSelf();
}

/*
 QoreNode::eval(): return value requires a dereference if needs_deref is true
 */
class QoreNode *QoreNode::eval(bool &needs_deref, ExceptionSink *xsink) const
{
   /*
     needs_deref = false;
     return const_cast<QoreNode *>(this);
    */

   if (type == NT_VARREF) 
      return val.vref->eval(needs_deref, xsink);

   if (!needs_eval())
   {
      needs_deref = false;
      return const_cast<QoreNode *>(this);
   }
   needs_deref = true;
   return eval(xsink);
}

int64 QoreNode::bigIntEval(ExceptionSink *xsink) const
{
   if (!needs_eval())
      return getAsBigInt();

   ReferenceHolder<QoreNode> rv(eval(xsink), xsink);
   if (*xsink || !rv)
      return 0;

   return rv->getAsBigInt();
}

int QoreNode::integerEval(ExceptionSink *xsink) const
{
   if (!needs_eval())
      return getAsInt();

   ReferenceHolder<QoreNode> rv(eval(xsink), xsink);
   if (*xsink || !rv)
      return 0;

   return rv->getAsInt();
}

bool QoreNode::boolEval(ExceptionSink *xsink) const
{
   if (type == NT_TREE)
      return val.tree->bool_eval(xsink);

   if (!needs_eval())
      return getAsBool();

   ReferenceHolder<QoreNode> rv(eval(xsink), xsink);
   if (*xsink || !rv)
      return false;

   return rv->getAsBool();
}

QoreString *QoreNode::getAsString(bool &del, int foff, class ExceptionSink *xsink) const
{
   if (type == NT_NOTHING) {
      del = false;
      return &NothingTypeString;
   }
   if (type == NT_NULL) {
      del = false;
      return &NullTypeString;
   }
   if (type == NT_FLOAT) {
      del = true;
      return new QoreString(val.floatval);
   }

   del = true;
   QoreString *rv = new QoreString();
   rv->sprintf("%s (0x%08p)", getTypeName(), this);
   return rv;
}

int QoreNode::getAsString(QoreString &str, int foff, class ExceptionSink *xsink) const
{
   if (type == NT_NOTHING)
      str.concat(&NothingTypeString);
   else if (type == NT_NULL)
      str.concat(&NullTypeString);
   else if (type == NT_FLOAT)
      str.concat(val.floatval);
   else 
      str.sprintf("%s (0x%08p)", getTypeName(), this);

   return 0;
}

bool QoreNode::getAsBool() const
{
   if (type == NT_FLOAT)
      return (bool)val.floatval;

   return false;
}

int QoreNode::getAsInt() const
{
   if (type == NT_FLOAT)
      return (int)val.floatval;

   return 0;
}

int64 QoreNode::getAsBigInt() const
{
   if (type == NT_FLOAT)
      return (int64)val.floatval;

   return 0;
}

double QoreNode::getAsFloat() const
{
   if (type == NT_FLOAT)
      return val.floatval;

   return 0.0;
}

// for getting relative time values or integer values
int getSecZeroInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return 0;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return (int)d->getRelativeSeconds();
   }
   return a->getAsInt();
}

int64 getSecZeroBigInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return 0;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return d->getRelativeSeconds();
   }
   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getSecMinusOneInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return -1;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return (int)d->getRelativeSeconds();
   }
   return a->getAsInt();
}

int64 getSecMinusOneBigInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return -1;
   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return d->getRelativeSeconds();
   }
   return a->getAsBigInt();
}

int getMsZeroInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return 0;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return d->getRelativeMilliseconds();
   }
   return a->getAsInt();
}

int64 getMsZeroBigInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return 0;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return d->getRelativeMilliseconds();
   }
   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getMsMinusOneInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return -1;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return (int)d->getRelativeMilliseconds();
   }
   return a->getAsInt();
}

int64 getMsMinusOneBigInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return -1;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return d->getRelativeMilliseconds();
   }
   return a->getAsBigInt();
}

int getMicroSecZeroInt(const class QoreNode *a)
{
   if (is_nothing(a))
      return 0;

   {
      const DateTimeNode *d = dynamic_cast<const DateTimeNode *>(a);
      if (d)
	 return (int)d->getRelativeMilliseconds() * 1000;
   }
   return a->getAsInt();
}

bool is_nothing(const QoreNode *n)
{
   if (!n || (n->type == NT_NOTHING))
      return true;
   
   const QoreObject *o = dynamic_cast<const QoreObject *>(n);
   if (o)
      return !o->isValid();
   
   return false;
}

static inline QoreListNode *crlr_list_copy(const QoreListNode *n, ExceptionSink *xsink)
{
   // if it's not an immediate list, then there can't be any
   // variable references in it at any level, so return copy
   if (!n->needs_eval()) {
      n->ref();
      return const_cast<QoreListNode *>(n);
   }

   // otherwise process each list element
   QoreListNode *l = new QoreListNode(true);
   for (int i = 0; i < n->size(); i++)
      l->push(copy_and_resolve_lvar_refs(n->retrieve_entry(i), xsink));
   return l;
}

static inline QoreNode *crlr_hash_copy(const QoreHashNode *n, ExceptionSink *xsink)
{
   // if it's not an immediate hash, then there can't be any
   // variable references in it at any level, so return copy
   if (!n->needsEval())
      return n->RefSelf();

   QoreHashNode *h = new QoreHashNode(1);
   ConstHashIterator hi(n);
   while (hi.next())
      h->setKeyValue(hi.getKey(), copy_and_resolve_lvar_refs(hi.getValue(), xsink), xsink);
   return h;
}

static inline QoreNode *crlr_tree_copy(const QoreNode *n, ExceptionSink *xsink)
{
   class Tree *t = new Tree(copy_and_resolve_lvar_refs(n->val.tree->left, xsink), n->val.tree->op, 
			    n->val.tree->right ? copy_and_resolve_lvar_refs(n->val.tree->right, xsink) : NULL);
   return new QoreNode(t);
}

static inline QoreNode *crlr_fcall_copy(const QoreNode *n, ExceptionSink *xsink)
{
   QoreNode *nn = new QoreNode(NT_FUNCTION_CALL);
   QoreListNode *na;
   if (n->val.fcall->args)
      na = crlr_list_copy(n->val.fcall->args, xsink);
   else
      na = NULL;

   switch (n->val.fcall->type)
   {
      case FC_USER:
	 nn->val.fcall = new FunctionCall(n->val.fcall->f.ufunc, na);
	 break;
      case FC_BUILTIN:
	 nn->val.fcall = new FunctionCall(n->val.fcall->f.bfunc, na);
	 break;
      case FC_SELF:
	 nn->val.fcall = new FunctionCall(n->val.fcall->f.sfunc->func, na);
	 break;
      case FC_UNRESOLVED:
	 nn->val.fcall = new FunctionCall(strdup(n->val.fcall->f.c_str), na);
	 break;
      case FC_IMPORTED:
	 nn->val.fcall = new FunctionCall(n->val.fcall->f.ifunc->pgm, n->val.fcall->f.ifunc->func, na);
	 break;
      case FC_METHOD:
	 nn->val.fcall = new FunctionCall(strdup(n->val.fcall->f.c_str), na);
	 nn->val.fcall->type = FC_METHOD;
	 break;
   }
   return nn;
}

static inline class QoreNode *eval_notnull(const class QoreNode *n, ExceptionSink *xsink)
{
   n = n->eval(xsink);
   if (!xsink->isEvent() && !n)
      return nothing();
   return const_cast<QoreNode *>(n);
}

class QoreNode *copy_and_resolve_lvar_refs(const QoreNode *n, ExceptionSink *xsink)
{
   if (!n) return NULL;

   {
      const QoreListNode *l = dynamic_cast<const QoreListNode *>(n);
      if (l)
	 return crlr_list_copy(l, xsink);
   }

   {
      const QoreHashNode *h = dynamic_cast<const QoreHashNode *>(n);
      if (h)
	 return crlr_hash_copy(h, xsink);
   }

   if (n->type == NT_TREE)
      return crlr_tree_copy(n, xsink);

   if (n->type == NT_FUNCTION_CALL)
      return crlr_fcall_copy(n, xsink);

   // must make sure to return a value here or it could cause a segfault - parse expressions expect non-NULL values for the operands
   if (n->type == NT_FIND || n->type == NT_SELF_VARREF)
      return eval_notnull(n, xsink);

   if (n->type == NT_VARREF && n->val.vref->type == VT_LOCAL)
      return eval_notnull(n, xsink);

   return n->RefSelf();
}

// get the value of the type in a string context, empty string for complex types (default implementation)
QoreString *QoreNode::getStringRepresentation(bool &del) const
{
   //del = false;
   //return null_string();

   // delete the following
   if (type == NT_FLOAT) {
      del = true;
      return new QoreString(val.floatval);
   }

   del = false;
   return NullString;
}

// empty default implementation
void QoreNode::getStringRepresentation(QoreString &str) const
{
   // delete this code
   if (type == NT_FLOAT)
      str.sprintf("%g", val.floatval);
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
class DateTime *QoreNode::getDateTimeRepresentation(bool &del) const
{
   // delete the following
   if (type == NT_FLOAT) {
      del = true;
      return new DateTime((int64)(val.floatval));
   }   

   del = false;
   return ZeroDate;
}

// assign date representation to a DateTime (no action for complex types = default implementation)
void QoreNode::getDateTimeRepresentation(DateTime &dt) const
{
   if (type == NT_FLOAT)
      dt.setDate((int64)(val.floatval));
   else
      dt.setDate(0LL);
}

bool QoreNode::is_equal_soft(const QoreNode *v, ExceptionSink *xsink) const
{
   if (is_nothing(this)) {
      if (is_nothing(v))
	 return true;
      return false;
   }
   if (is_nothing(v))
      return false;

   if (is_null(this))
      if (is_null(v))
	 return true;
      else
	 return false;

   if (type == NT_FLOAT)
      return val.floatval == v->getAsFloat();
   // the following types can't be converted
   if (type != v->type)
      return false;

   assert(false);
   // FIXME: pure virtual function!
   return false;
}

bool QoreNode::is_equal_hard(const QoreNode *v, ExceptionSink *xsink) const
{
   if (type != v->type)
      return false;

   if (is_nothing(this))
      return true;

   if (is_null(this))
      return true;

   if (type == NT_FLOAT)
      return val.floatval == v->val.floatval;

   assert(false);   
   // FIXME: pure virtual function!
   return false;
}

// returns the data type
const QoreType *QoreNode::getType() const
{
   return type;
}

const char *QoreNode::getTypeName() const
{
   return type->getName();
}

SimpleQoreNode::SimpleQoreNode(const QoreType *t) : QoreNode(t)
{
}

void SimpleQoreNode::deref()
{
   if (ROdereference())
      delete this;   
}

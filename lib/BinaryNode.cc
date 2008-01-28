/*
  BinaryNode.cc

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

#include <string.h>
#include <stdlib.h>

BinaryNode::BinaryNode(void *p, unsigned size) : SimpleQoreNode(NT_BINARY)
{
   ptr = p;
   len = size;
}

BinaryNode::~BinaryNode()
{
   if (ptr)
      free(ptr);
}

// returns 0 = equal, 1 = not equal
int BinaryNode::compare(const BinaryNode *obj) const
{
   // if the sizes are not equal, then the objects can't be equal
   if (len != obj->len)
      return 1;

   // if both are zero, then they are equal
   if (!len)
      return 0;

   return memcmp(ptr, obj->ptr, len);
}

unsigned BinaryNode::size() const
{
   return len;
}

BinaryNode *BinaryNode::copy() const
{
   if (!len)
      return new BinaryNode();

   void *np = malloc(len);
   memcpy(np, ptr, len);
   return new BinaryNode(np, len);
}

const void *BinaryNode::getPtr() const
{
   return ptr;
}

void BinaryNode::append(const void *nptr, unsigned size)
{
   ptr = realloc(ptr, len + size);
   memcpy((char *)ptr + len, nptr, size);
   len += size;
}

void BinaryNode::append(const BinaryNode *b)
{
   append(b->ptr, b->len);
}

void *BinaryNode::giveBuffer()
{
   void *p = ptr;
   ptr = NULL;
   len = 0;
   return p;
}

// FIXME: move QoreString * to first argument
// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
QoreString *BinaryNode::getAsString(bool &del, int foff, class ExceptionSink *xsink) const
{
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

int BinaryNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const
{
   str.sprintf("binary object %08p (%d byte%s)", getPtr(), size(), size() == 1 ? "" : "s");
   return 0;
}

QoreNode *BinaryNode::realCopy() const
{
   return copy();
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL virtual int compare(const QoreNode *val) const;
// the type passed must always be equal to the current type
bool BinaryNode::is_equal_soft(const QoreNode *v, ExceptionSink *xsink) const
{
   const BinaryNode *b = dynamic_cast<const BinaryNode *>(v);
   if (!b)
      return false;
   return !compare(b);
}

bool BinaryNode::is_equal_hard(const QoreNode *v, ExceptionSink *xsink) const
{
   return is_equal_soft(v, xsink);
}

// returns the data type
const QoreType *BinaryNode::getType() const
{
   return NT_BINARY;
}

// returns the type name as a c string
const char *BinaryNode::getTypeName() const
{
   return "binary";
}


/*
  QT_list.cc

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
#include <qore/intern/QT_list.h>

class QoreNode *list_DefaultValue()
{
   emptyList->ref();
   return emptyList;
}

class QoreNode *list_ConvertTo(class QoreNode *n, class ExceptionSink *xsink)
{
   QoreList *l = new QoreList();
   l->push(n ? n->eval(xsink) : NULL);
   return new QoreNode(l);
}

bool list_needs_eval(class QoreNode *n)
{
   return n->val.list->needsEval();
}

class QoreNode *list_Eval(class QoreNode *l, class ExceptionSink *xsink)
{
   if (!l->val.list->needsEval())
      return l->RefSelf();

   return l->val.list->eval(xsink);
}

class QoreNode *list_eval_opt_deref(bool &needs_deref, class QoreNode *n, class ExceptionSink *xsink)
{
   if (!n->val.list->needsEval())
   {
      needs_deref = false;
      return n;
   }

   needs_deref = true;
   return n->val.list->eval(xsink);
}

class QoreNode *list_Copy(class QoreNode *l, class ExceptionSink *xsink)
{
   return l->val.list->eval(xsink);
}

bool list_Compare(class QoreNode *l, class QoreNode *r, class ExceptionSink *xsink)
{
   if (l->val.list->size() != r->val.list->size())
      return 1;
   for (int i = 0; i < l->val.list->size(); i++)
      if (compareHard(l->val.list->retrieve_entry(i), r->val.list->retrieve_entry(i), xsink))
	 return 1;
   return 0;
}

class QoreString *list_MakeString(class QoreNode *n, int foff, class ExceptionSink *xsink)
{
   QoreString *rv = new QoreString();
   if (!n->val.list->size())
      rv->concat("<EMPTY LIST>");
   else
   {
      rv->concat("list: ");
      if (foff != FMT_NONE)
	 rv->sprintf("(%d element%s)\n", n->val.list->size(), n->val.list->size() == 1 ? "" : "s");
      else
	 rv->concat('(');
      for (int i = 0; i < n->val.list->size(); i++)
      {
	 if (foff != FMT_NONE)
	 {
	    rv->addch(' ', foff + 2);
	    rv->sprintf("[%d]=", i);
	 }
	 QoreString *elem = n->val.list->retrieve_entry(i)->getAsString(foff != FMT_NONE ? foff + 2 : foff, xsink);
	 rv->concat(elem);
	 delete elem;
	 if (i != (n->val.list->size() - 1))
	    if (foff != FMT_NONE)
	       rv->concat('\n');
	    else
	       rv->concat(", ");
      }
      if (foff == FMT_NONE)
	 rv->concat(')');
   }
   return rv;
}

void list_DeleteContents(class QoreNode *n)
{
   n->val.list->derefAndDelete(NULL);
}

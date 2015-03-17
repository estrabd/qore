/*
  FunctionList.cpp
 
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
#include <qore/intern/FunctionList.h>

#include <string.h>

ResolvedCallReferenceNode* FunctionEntry::makeCallReference() const {
   return new LocalFunctionCallReferenceNode(func);
}

FunctionList::FunctionList(const FunctionList& old, qore_ns_private* ns, int64 po) {
   bool no_user = po & PO_NO_INHERIT_USER_FUNC_VARIANTS;
   bool no_builtin = po & PO_NO_SYSTEM_FUNC_VARIANTS;
   for (fl_map_t::const_iterator i = old.begin(), e = old.end(); i != e; ++i) {
      QoreFunction* f = i->second->getFunction();
      if (!f->hasBuiltin()) {
         if (no_user || !f->hasUserPublic())
            continue;
      }
      else if (no_builtin && !f->hasUserPublic())
         continue;

      FunctionEntry* fe = new FunctionEntry(i->first, new QoreFunction(*f, po, ns));
      insert(std::make_pair(fe->getName(), fe));
      //if (!strcmp(i->first, "make_select_list2"))
      //if (f->hasUser())  printd(0, "FunctionList::FunctionList() this: %p copying fe: %p %s user: %d builtin: %d public: %d\n", this, i->second, i->first, f->hasUser(), f->hasBuiltin(), f->hasUserPublic());
   }
}

void FunctionList::del() {
   for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
      delete i->second;
   clear();
   assert(empty());
}

FunctionEntry* FunctionList::add(QoreFunction* func) {
   QORE_TRACE("FunctionList::add()");
   assert(!findNode(func->getName()));
   assert(func->getNamespace());

   FunctionEntry* n = new FunctionEntry(func);
   insert(std::make_pair(func->getName(), n));
   return n;
}

FunctionEntry* FunctionList::import(QoreFunction* func, qore_ns_private* ns) {
   QORE_TRACE("FunctionList::import()");
   assert(!findNode(func->getName()));
   assert(func->getNamespace());

   // copy function entry for import and insert into map
   FunctionEntry* fe = new FunctionEntry(new QoreFunction(*func, 0, ns));
   insert(std::make_pair(fe->getName(), fe));
   return fe;
}

FunctionEntry* FunctionList::import(const char* new_name, QoreFunction* func, qore_ns_private* ns) {
   QORE_TRACE("FunctionList::add()");

   assert(!findNode(new_name));

   // copy function entry for import and insert into map
   FunctionEntry* fe = new FunctionEntry(new_name, new QoreFunction(*func, 0, ns, true));
   insert(std::make_pair(fe->getName(), fe));
   return fe;
}

FunctionEntry* FunctionList::findNode(const char* name) const {
   printd(5, "FunctionList::findNode(%s)\n", name);

   fl_map_t::const_iterator i = fl_map_t::find(name);
   return i != end() ? i->second : 0;
}

QoreFunction* FunctionList::find(const char* name, bool runtime) const {
   printd(5, "FunctionList::findFunction(%s) (QoreFunction)\n", name);

   fl_map_t::const_iterator i = fl_map_t::find(name);
   if (i != end())
      return i->second->getFunction(runtime);

   return 0;
}

QoreListNode* FunctionList::getList() {
   QoreListNode* l = new QoreListNode;

   for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
      l->push(new QoreStringNode(i->first));      

   return l;
}

void FunctionList::parseInit() {
   for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
      i->second->parseInit();
}

void FunctionList::parseCommit() {
   // commit pending variants in all functions
   for (fl_map_t::iterator i = begin(), e = end(); i != e; ++i)
      i->second->parseCommit();
}

void FunctionList::parseRollback() {
   for (fl_map_t::iterator i = begin(), e = end(); i != e;) {
      if (i->second->parseRollback()) {
	 delete i->second;
	 erase(i++);
	 continue;
      }

      ++i;
   }
}

void FunctionList::assimilate(FunctionList& fl, qore_ns_private* ns) {
   for (fl_map_t::iterator i = fl.begin(), e = fl.end(); i != e;) {
      fl_map_t::const_iterator li = fl_map_t::find(i->first);
      if (li == end()) {
	 insert(fl_map_t::value_type(i->first, i->second));
	 i->second->updateNs(ns);
      }
      else {
	 li->second->getFunction()->parseAssimilate(*(i->second->getFunction()));
	 delete i->second;
      }

      fl.erase(i++);
   }   
}

int FunctionList::importSystemFunctions(const FunctionList& src, qore_ns_private* ns) {
   int cnt = 0;
   for (fl_map_t::const_iterator i = src.begin(), e = src.end(); i != e; ++i) {
      if (!i->second->hasBuiltin() || fl_map_t::find(i->second->getName()) != fl_map_t::end())
	 continue;

      FunctionEntry* fe = new ModuleImportedFunctionEntry(*i->second, ns);
      //printd(5, "FunctionList::importSystemFunctions() this: %p merging in %s (%p)\n", this, i->first, fe);
      insert(fl_map_t::value_type(fe->getName(), fe));
      ++cnt;
   }
   return cnt;
}

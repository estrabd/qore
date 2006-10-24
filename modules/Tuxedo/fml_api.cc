/*
  modules/Tuxedo/fml_api.cc

  Tuxedo integration to QORE

  Qore Programming Language

  Copyright (C) 2006 Qore Technologies

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

#include <qore/common.h>
#include <qore/support.h>
#include <qore/Exception.h>
#include <qore/Namespace.h>
#include <qore/BuiltinFunctionList.h>
#include <qore/params.h>
#include <qore/QoreString.h>
#include <qore/LockedObject.h>

#include "fml_api.h"
#include <fml32.h> // must be before <fml.h>
#include <fml.h>
#include <atmi.h>
#include "ScopeGuard.h"
#include "QoreTuxedoTypedBuffer.h"
#include "QC_TuxedoTypedBuffer.h"

#include <limits.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <utility>
#include <memory>

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using std::auto_ptr;

//------------------------------------------------------------------------------
// n - known as an object
static QoreTuxedoTypedBuffer* node2typed_buffer(QoreNode* n, char* func_name, ExceptionSink* xsink)
{
  if (!n->val.object) {
    xsink->raiseException(func_name, "Expected instance of Tuxedo::TuxedoTypedBuffer, NULL found.");
    return 0;
  }
  if (n->val.object->getClass()->getID() != CID_TUXEDOTYPEDBUFFER) {
    xsink->raiseException(func_name, "Type mismatch: expected instance of Tuxedo::TuxedoTypedBuffer class.");
    return 0;
  }
  // this should be safe now
  QoreTuxedoTypedBuffer* buff = (QoreTuxedoTypedBuffer*)(n->val.object);
  return buff;
}

//-----------------------------------------------------------------------------
// Returns previous value (empty if none)
static string set_env_variable(const char* name, const char* value)
{
  const char* old = getenv(name);
  string result(old ? old : "");
  if (value && value[0]) {
    setenv(name, value, 1);
  } else {
   unsetenv(name);
  }
  return result;
}

//-----------------------------------------------------------------------------
// Serialized by mutex
static vector<pair<string, string> > set_env_variables(const vector<pair<string, string> >& vars)
{
  static LockedObject mutex;
  mutex.lock();
  ON_BLOCK_EXIT_OBJ(mutex, &LockedObject::unlock);

  vector<pair<string, string> > result;
  for (unsigned i = 0; i < vars.size(); ++i) {
    string old = set_env_variable(vars[i].first.c_str(), vars[i].second.c_str());
    result.push_back(make_pair(vars[i].first, old));
  }
  return result;
}

//-----------------------------------------------------------------------------
// See http://edocs.bea.com/tuxedo/tux91/fml/fml04.htm#101389
static vector<pair<string, string> > set_fml_env_variables(const vector<string>& files, bool is_fml32)
{
  string joined_files;
  for (unsigned i = 0, n = files.size(); i != n; ++i) {
    joined_files += files[i];
    if (i + 1 != n) joined_files += ","; // that's the Tuxedo way
  }

  vector<pair<string, string> > new_env;
  new_env.push_back(make_pair(is_fml32 ? "FIELDTBLS32" : "FIELDTBLS", joined_files));
  new_env.push_back(make_pair(is_fml32 ? "FLDTBLDIR32" : "FLDTBLDIR", string())); // not used, all files need to be fullpath
  
  return set_env_variables(new_env);
}

//-----------------------------------------------------------------------------
// Extract all names from given table description file.
// See http://edocs.bea.com/tuxedo/tux91/fml/fml04.htm#1010346
static vector<string> read_names_from_fml_description_file(const char* filename, ExceptionSink* xsink)
{
   vector<string> result;
  FILE* f = fopen(filename, "rt");
  if (!f) {
    xsink->raiseException("FML[32]_process_description_tables", "read_names_from_fml_description_file(): the file [ %s ] cannot be opened.", filename);
    return result;
  }
  ON_BLOCK_EXIT(fclose, f);

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    char* thumb = line;
    while (isspace(*thumb)) ++thumb;
    if (!*thumb) continue;
    if (strstr(thumb, "*base") == thumb) continue;
    if (*thumb == '#') continue;
   
     char* name_end = thumb;
     while (!isspace(*name_end) && *name_end) ++name_end;
     *name_end = 0;
    
     if (name_end == thumb) continue; // ???
     result.push_back(thumb);
  }
  return result;
}

//-----------------------------------------------------------------------------
// Extract all names from all given table description file.
static vector<string> read_names_from_all_fml_description_files(const vector<string>& files, ExceptionSink* xsink)
{
   vector<string> result;
   for (unsigned i = 0; i < files.size(); ++i) {
     vector<string> aux = read_names_from_fml_description_file(files[i].c_str(), xsink);
     if (xsink->isException()) {
       return vector<string>();
     }
     result.insert(result.end(), aux.begin(), aux.end());
   }
   return result;
}

//-----------------------------------------------------------------------------
// Return files passed into FML[32]_process_description_tables()
static vector<string> extract_file_names(QoreNode* params, ExceptionSink* xsink)
{
  typedef vector<string> result_t;
  char* name = "FML[32]_process_description_tables";
  char* params_err = "One parameter, filename string or list of filenames, expected.";

  if (!get_param(params, 0)) {
    xsink->raiseException(name, params_err);
    return result_t();
  }
  if (get_param(params, 1)) {
    xsink->raiseException(name, params_err);
    return result_t();
  }

  vector<string> files;

  QoreNode* n = test_param(params, NT_STRING, 0);
  if (n) {
    char* filename = n->val.String->getBuffer();
    if (!filename || !filename[0]) {
      xsink->raiseException(name, "Empty string cannot be passed as the filename.");
      return result_t();
    }
    files.push_back(filename);
  } else {
    n = test_param(params, NT_LIST, 0);
    if (!n) {
      xsink->raiseException(name, params_err);
      return result_t();
    }
    List* l = n->val.list;
    int cnt = l->size();
    for (int i = 0; i < cnt; ++i) {
      n = l->retrieve_entry(i);
      if (!n) {
        xsink->raiseException(name, "The list of files cannot contain empty item.");
        return result_t();
      }
      if (n->type != NT_STRING) {
        xsink->raiseException(name, "The list needs to contains file names as strings.");
        return result_t();
      }
      char* filename = n->val.String->getBuffer();
      if (!filename || !filename[0]) {
        xsink->raiseException(name, "Empty string cannot be passed as the filename.");
        return result_t();
      }
      files.push_back(filename);
    }
  }
  if (files.empty()) {
    xsink->raiseException(name, "At least one file needs to be specified.");
    return result_t();
  }
  return files;
}

//-----------------------------------------------------------------------------
static QoreNode* process_description_tables(QoreNode* params, ExceptionSink* xsink, bool is_fml32)
{
  vector<string> files = extract_file_names(params, xsink);
  if (xsink->isException()) {
    return 0;
  }

  vector<string> all_names = read_names_from_all_fml_description_files(files, xsink);
  if (xsink->isException()) {
    return 0;
  }

  vector<pair<string, string> > old_env = set_fml_env_variables(files, is_fml32);

  // before returning the old variables back free the tables from memory
  // (assumption: Fldid[32] is idempotent)
  ON_BLOCK_EXIT((is_fml32 ? &Fidnm_unload : &Fidnm_unload32));
  ON_BLOCK_EXIT(set_env_variables, old_env);

  auto_ptr<Hash> result(new Hash);

  char* func_name = "FML[32]_process_description_tables";

  for (unsigned i = 0, n = all_names.size(); i != n; ++i) {
    char* name = (char*)all_names[i].c_str();
    FLDID32 id;
    if (is_fml32) {
      id = Fldid32(name);
    } else {
      id = Fldid(name);
    }
    if (id == BADFLDID) {
      xsink->raiseException(func_name, "Fldid[32](\"%s\") failed. Ferror = %d.", name, Ferror);
      return 0;
    }
    int type;
    if (is_fml32) {
      type = Fldtype32(id);
    } else {
      type = Fldtype(id);
    }
    List* list = new List();
    list->insert(new QoreNode((int64)id));
    list->insert(new QoreNode((int64)type));
    result->setKeyValue(name, new QoreNode(list), xsink);
    if (xsink->isException()) {
      return 0;
    }
  }
  return new QoreNode(result.release());
}

//-----------------------------------------------------------------------------
static QoreNode* f_fml_process_description_tables(QoreNode* params, ExceptionSink* xsink)
{
  return process_description_tables(params, xsink, false);
}

//-----------------------------------------------------------------------------
static QoreNode* f_fml32_process_description_tables(QoreNode* params, ExceptionSink* xsink)
{
  return process_description_tables(params, xsink, true);
}

//-----------------------------------------------------------------------------
static void extract_hashes(QoreNode* params, ExceptionSink* xsink, Hash*& fml_settings, Hash*& fml_values, 
  QoreTuxedoTypedBuffer*& buffer, char* func_name)
{
  for (int i = 0; i <= 3; ++i) {
    bool ok;
    if (i == 3) ok = !get_param(params, i);
    else ok = get_param(params, i);
    if (!ok) {
      xsink->raiseException(func_name, "Three paramaters (two hashes, instance of TuxedoTypedBuffer) expected: FML[32] settings, named values and out typed buffer passed by reference.");
      return;
    }
  }

  QoreNode* n = test_param(params, NT_HASH, 0);
  if (!n) {
    xsink->raiseException(func_name, "The first parameter, FML[32] settings, needs to be a hash, possibly passed by reference.");
    return;
  }
  fml_settings = n->val.hash;

  n = test_param(params, NT_HASH, 1);
  if (!n) {
    xsink->raiseException(func_name, "The second parameter, FML[32] values, needs to be a hash, possibly passed by reference.");
    return;
  }
  fml_values = n->val.hash;

  n = test_param(params, NT_OBJECT, 2);
  if (!n) {
    xsink->raiseException(func_name, "The third parameter needs to be  TuxedoTypedBuffer instance passed by reference.");
    return;
  }
  buffer = node2typed_buffer(n, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void extract_hashes(QoreNode* params, ExceptionSink* xsink, Hash*& fml_settings, 
  QoreTuxedoTypedBuffer*& buffer, char* func_name)
{
  for (int i = 0; i <= 2; ++i) {
    bool ok;
    if (i == 2) ok = !get_param(params, i);
    else ok = get_param(params, i);
    if (!ok) {
      xsink->raiseException(func_name, "Two paramaters (hash, instance of TuxedoTypedBuffer) expected: FML[32] settings and the typed buffer.");
      return;
    }
  }

  QoreNode* n = test_param(params, NT_HASH, 0);
  if (!n) {
    xsink->raiseException(func_name, "The first parameter, FML[32] settings, needs to be a hash, possibly passed by reference.");
    return;
  }
  fml_settings = n->val.hash;

  n = test_param(params, NT_OBJECT, 1);
  if (!n) {
    xsink->raiseException(func_name, "The second parameter needs to be  TuxedoTypedBuffer instance possibly passed by reference.");
    return;
  }
  buffer = node2typed_buffer(n, func_name, xsink);
}

//-----------------------------------------------------------------------------
// Find out ID and type from FML name
static pair<FLDID32, int> fml_name2id(const char* name, Hash* fml_settings, ExceptionSink* xsink, char* func_name)
{
  pair<FLDID32, int> result(0, 0);

  QoreNode* n = fml_settings->getKeyValue((char*)name);
  if (!n) {
    xsink->raiseException(func_name, "Field [ %s ] does not exist in FML[32] settings. Please check possible typos.");
    return result;
  }
  if (n->type != NT_LIST) {
    xsink->raiseException(func_name, "Invalid FML[32] settings for [ %s ]: a list expected.");
    return result;
  }
  List* l = n->val.list;
  if (l->size() != 2) {
    xsink->raiseException(func_name, "Invalid FML[32] settings for [ %s ]: a list with 2 items expected.");
    return result;
  }
  n = l->retrieve_entry(0);
  if (!n || n->type != NT_INT) {
    xsink->raiseException(func_name, "Invalid FML[32] settings for [ %s ]: the first item in list needs to be an integer.");
    return result;
  }
  result.first = (FLDID32)n->val.intval;
  n = l->retrieve_entry(1);
  if (!n || n->type != NT_INT) {
    xsink->raiseException(func_name, "Invalid FML[32] settings for [ %s ]: the second item in list needs to be an integer.");
    return result;
  }
  result.second = (int)n->val.intval;

  return result;
}

//-----------------------------------------------------------------------------
static int64 node2int(char* field_name, QoreNode* node, char* func_name, ExceptionSink* xsink)
{
  if (node->type == NT_BOOLEAN) {
    return node->val.boolval ? 1 : 0;
  }
  if (node->type == NT_INT) {
    return node->val.intval;
  }
  xsink->raiseException(func_name, "Value [ %s ] cannot be converted to expected numeric value.");
  return 0;
}

//-----------------------------------------------------------------------------
static double node2double(char* field_name, QoreNode* node, char* func_name, ExceptionSink* xsink)
{
  if (node->type == NT_BOOLEAN) {
    return node->val.boolval ? 1.0 : 0.0;
  }
  if (node->type == NT_INT) {
    double d = node->val.intval;
    return d;
  }
  if (node->type == NT_FLOAT) {
    return node->val.floatval;
  }
  xsink->raiseException(func_name, "Value [ %s ] cannot be converted to expected floating point value.");
  return 0;
}

//-----------------------------------------------------------------------------
static void append_value_in_buffer(bool is_fml32, char* field_name, char* value, 
  int len, FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  for (;;) {
    int res;
    if (is_fml32) {
      res = Fappend32((FBFR32*)buff->buffer, id, value, len);
    } else {
      res = Fappend((FBFR*)buff->buffer, id, value, len);
    }
    if (res != -1) {
      break;
    }
    if (Ferror != FNOSPACE) {
      xsink->raiseException(func_name, "Value [ %s ] cannot be appended into FML[32] buffer. Error %d.", field_name, Ferror);
      return;
    }
    // the buffer needs to be resized
    int increment = buff->size;
    if (increment > 64 * 1024) {
      increment = 64 * 1024; // do not resize too wildly
    }
    char* new_buffer = tprealloc(buff->buffer, buff->size + increment);
    if (new_buffer) {
      buff->buffer = new_buffer;
      buff->size += increment;
      continue;
    }
    xsink->raiseException(func_name, "Failed to reallocate typed buffer. Tuxedo error %d.", tperrno);
    return;
  }

}

//-----------------------------------------------------------------------------
static void append_fml_short_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  int64 val = node2int(field_name, field_value, func_name, xsink);
  if (xsink->isException()) {
    return;
  }
  if (val < SHRT_MIN || val > SHRT_MAX) {
    xsink->raiseException(func_name, "The value [ %s ] is out of range for short int.", field_name);
    return;
  }
  short int si = (short int)val;

  append_value_in_buffer(is_fml32, field_name, (char*)&si, sizeof(si), id, buff, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void append_fml_long_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  int64 val = node2int(field_name, field_value, func_name, xsink);
  if (xsink->isException()) {
    return;
  }
  if (val < LONG_MIN || val > LONG_MAX) {
    xsink->raiseException(func_name, "The value [ %s ] is out of range for long int.", field_name);
    return;
  }
  long int li = (long int)val;

  append_value_in_buffer(is_fml32, field_name, (char*)&li, sizeof(li), id, buff, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void append_fml_char_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  int64 val = node2int(field_name, field_value, func_name, xsink);
  if (xsink->isException()) {
    return;
  }
  if (val < CHAR_MIN || val > CHAR_MAX) {
    xsink->raiseException(func_name, "The value [ %s ] is out of range for character.", field_name);
    return;
  }
  char ch = (char)val;

  append_value_in_buffer(is_fml32, field_name, &ch, sizeof(ch), id, buff, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void append_fml_float_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  double val = node2double(field_name, field_value, func_name, xsink);
  if (xsink->isException()) {
    return;
  }
  float f = val;

  append_value_in_buffer(is_fml32, field_name, (char*)&f, sizeof(f), id, buff, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void append_fml_double_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  double val = node2double(field_name, field_value, func_name, xsink);
  if (xsink->isException()) {
    return;
  }

  append_value_in_buffer(is_fml32, field_name, (char*)&val, sizeof(val), id, buff, func_name, xsink);
}

//-----------------------------------------------------------------------------
static void append_fml_string_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{

  // TBD
}

//-----------------------------------------------------------------------------
static void append_fml_binary_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  // TBD
}

//-----------------------------------------------------------------------------
static void append_fml_fml32_in_buffer(char* field_name, QoreNode* field_value,
  FLDID32 id, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  // TBD
}

//-----------------------------------------------------------------------------
static void append_fml_field_in_buffer(bool is_fml32, char* field_name, QoreNode* field_value, 
  FLDID32 id, int type, QoreTuxedoTypedBuffer* buff, char* func_name, ExceptionSink* xsink)
{
  switch (type) {
  case FLD_SHORT:
    append_fml_short_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_LONG:
    append_fml_long_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_CHAR:
    append_fml_char_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_FLOAT:
    append_fml_float_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_DOUBLE:
    append_fml_double_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_STRING:
    append_fml_string_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_CARRAY:
    append_fml_binary_in_buffer(is_fml32, field_name, field_value, id, buff, func_name, xsink);
    break;
  case FLD_FML32:
    if (!is_fml32) {
      xsink->raiseException(func_name, "Field name [ %s ]: FML32 data cannot be added into FML buffer.", field_name);
      break;
    }
    append_fml_fml32_in_buffer(field_name, field_value, id, buff, func_name, xsink);
    break;

  case FLD_PTR:
  case FLD_VIEW32:
  case FLD_MBSTRING:
   xsink->raiseException(func_name, "Field name [ %s ]: type %d is not yet supported.", field_name, type);
   break;
  default:
    xsink->raiseException(func_name, "Field name [ %s ] has unknown type %d.", field_name, type);
    break;
  }
}

//-----------------------------------------------------------------------------
static QoreNode* write_into_buffer(QoreNode* params, ExceptionSink* xsink, bool is_fml32)
{
  char* func_name = "putFML[32]InTypedBuffer";

  Hash* fml_settings = 0;
  Hash* fml_values = 0;
  QoreTuxedoTypedBuffer* buff = 0;
  extract_hashes(params, xsink, fml_settings, fml_values, buff, func_name);
  if (xsink->isException()) {
    return 0;
  }

  buff->clear();
  const int InitialBufferSize = 4096;
  char* type = (char*)(is_fml32 ? "FML32" : "FML");
  buff->buffer = tpalloc(type, 0, InitialBufferSize);
  if (!buff->buffer) {
    xsink->raiseException(func_name, "tpalloc() failed with error%d.", tperrno);
    return 0;
  }
  buff->size = InitialBufferSize;

  int res;
  if (is_fml32) {
    res = Finit32((FBFR32*)buff->buffer, buff->size);
  } else {
    res = Finit((FBFR*)buff->buffer, buff->size);
  }
  if (res == -1) {
    xsink->raiseException(func_name, "Fini[32] failed with error %d.", Ferror);
    return 0;
  }

  // iterate through values and add them into the buffer
  HashIterator it(fml_values);
  int cnt = fml_values->size();
  for (int i = 0; i < cnt; ++i) {
   char* field_name = it.getKey();
   QoreNode* field_value = it.getValue();

    pair<FLDID32, int> id = fml_name2id(field_name, fml_settings, xsink, func_name);
    if (xsink->isException()) {
      return 0;
    }
    append_fml_field_in_buffer(is_fml32, field_name, field_value, id.first, id.second, buff, func_name, xsink);
    if (xsink->isException()) {
      return 0;
    }

    it.next();
  }

  // set up proper size as a check and to switch away from append mode
  long result_size;
  if (is_fml32) {
    result_size = Fsizeof32((FBFR32*)buff->buffer);
  } else {
    result_size = Fsizeof((FBFR*)buff->buffer);
  }
  if (result_size == -1) {
    xsink->raiseException(func_name, "Fsizeof[32] failed with error %d.", Ferror);
    return 0;
  }
  buff->size = (int)result_size;

  return 0;
}

//-----------------------------------------------------------------------------
static QoreNode* get_from_buffer(QoreNode* params, ExceptionSink* xsink, bool is_fml32)
{
  char* func_name = "getFML[32]FromTypedBuffer";
  Hash* fml_settings = 0;
  QoreTuxedoTypedBuffer* buff = 0;
  extract_hashes(params, xsink, fml_settings, buff, func_name);
  if (xsink->isException()) {
    return 0;
  }
  if (!buff->buffer) {
    xsink->raiseException(func_name, "The typed buffer is empty.");
    return 0;
  }
  char type[20];
  char subtype[20];
  if (tptypes(buff->buffer, type, subtype) == -1) {
    xsink->raiseException(func_name, "tptypes() of typed buffer failed with error %d.", tperrno);
    return 0;
  }
  const char* expected_type = is_fml32 ? "FML32" : "FML";
  if (strcmp(type, expected_type)) {
    xsink->raiseException(func_name, "Unexpected type of data in typed buffer: %s expected, %s found.", expected_type, type);
    return 0;
  }
  // is the buffer actually FML[32], i.e. fielded?
  bool fielded;
  if (is_fml32) {
    fielded = Fielded32((FBFR32*)buff->buffer);
  } else {
    fielded = Fielded((FBFR*)buff->buffer);
  }
  if (!fielded) {
    xsink->raiseException(func_name, "The typed buffer doesn't contain valid FML/FML32 data.");
    return 0;
  }

  return 0; // TBD
}


//-----------------------------------------------------------------------------
static QoreNode* f_fml_write_into_buffer(QoreNode* params, ExceptionSink* xsink)
{
  return write_into_buffer(params, xsink, false);
}

//-----------------------------------------------------------------------------
static QoreNode* f_fml32_write_into_buffer(QoreNode* params, ExceptionSink* xsink)
{
  return write_into_buffer(params, xsink, true);
}

//-----------------------------------------------------------------------------
static QoreNode* f_fml_get_from_buffer(QoreNode* params, ExceptionSink* xsink)
{
  return get_from_buffer(params, xsink, false);
}

//-----------------------------------------------------------------------------
static QoreNode* f_fml32_get_from_buffer(QoreNode* params, ExceptionSink* xsink)
{
  return get_from_buffer(params, xsink, true);
}

//-----------------------------------------------------------------------------
void tuxedo_fml_init()
{
 builtinFunctions.add("processFMLDescripionTables", f_fml_process_description_tables, QDOM_NETWORK);
 builtinFunctions.add("processFML32DescripionTables", f_fml32_process_description_tables, QDOM_NETWORK);

 builtinFunctions.add("putFMLInTypedBuffer", f_fml_write_into_buffer, QDOM_NETWORK);
 builtinFunctions.add("putFML32InTypedBuffer", f_fml32_write_into_buffer, QDOM_NETWORK);
 builtinFunctions.add("getFMLFromTypedBuffer", f_fml_get_from_buffer, QDOM_NETWORK);
 builtinFunctions.add("getFML32FromTypedBuffer", f_fml32_get_from_buffer, QDOM_NETWORK);
}

//-----------------------------------------------------------------------------
void tuxedo_fml_ns_init(Namespace* ns)
{
 // FML/FML32 types
  ns->addConstant("FLD_SHORT", new QoreNode((int64)FLD_SHORT));
  ns->addConstant("FLD_LONG", new QoreNode((int64)FLD_LONG));
  ns->addConstant("FLD_CHAR", new QoreNode((int64)FLD_CHAR));
  ns->addConstant("FLD_FLOAT", new QoreNode((int64)FLD_FLOAT));
  ns->addConstant("FLD_DOUBLE", new QoreNode((int64)FLD_DOUBLE));
  ns->addConstant("FLD_STRING", new QoreNode((int64)FLD_STRING));
  ns->addConstant("FLD_CARRAY", new QoreNode((int64)FLD_CARRAY));
  ns->addConstant("FLD_PTR", new QoreNode((int64)FLD_PTR));
  ns->addConstant("FLD_FML32", new QoreNode((int64)FLD_FML32));
  ns->addConstant("FLD_VIEW32", new QoreNode((int64)FLD_VIEW32));
  ns->addConstant("FLD_MBSTRING", new QoreNode((int64)FLD_MBSTRING));
}

// EOF


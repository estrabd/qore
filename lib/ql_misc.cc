/*
  ql_misc.cc

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
#include <qore/intern/ql_misc.h>
#include <qore/intern/QT_backquote.h>
#include <qore/intern/ssl_constants.h>
#include <qore/intern/QoreSignal.h>

#include <string.h>
#include <zlib.h>
#include <time.h>

#ifdef HAVE_GZ_HEADER
class qore_gz_header : public gz_header
{
      DLLLOCAL qore_gz_header(bool n_text, char *n_name, char *n_comment)
      {
	 text = n_text;
	 time = ::time(NULL);
	 xflags = 0;
	 os = 3; // always set to UNIX
	 extra = Z_NULL;
	 extra_len = 0;
	 extra_max = 0;
	 name = (Bytef *)n_name;
	 name_max = 0;
	 comment = (Bytef *)n_comment;
	 comm_max = 0;
	 hcrc = 0;
	 done = 0;
      }
};
#endif

static class QoreNode *f_call_function(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0;

   p0 = get_param(params, 0);
   if (!p0 || (p0->type != NT_FUNCREF && p0->type != NT_STRING))
   {
      xsink->raiseException("CALL-FUNCTION-PARAMETER-ERROR",
			    "invalid argument passed to call_function(), first argument must be either function name or call reference");
      return NULL;
   }

   ReferenceHolder<QoreListNode> args(xsink);
   // if there are arguments to pass, create argument list by copying current list
   if (num_params(params) > 1)
      args = params->copyListFrom(1);

   {
      QoreStringNode *str = dynamic_cast<QoreStringNode *>(p0);
      if (str)
	 return getProgram()->callFunction(str->getBuffer(), *args, xsink);
   }

   return p0->val.funcref->exec(*args, xsink);
}

static class QoreNode *f_call_function_args(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0, *p1;
   QoreListNode *args;

   p0 = get_param(params, 0);
   if (!p0 || (p0->type != NT_FUNCREF && p0->type != NT_STRING))
   {
      xsink->raiseException("CALL-FUNCTION-ARGS-PARAMETER-ERROR",
			    "invalid argument passed to call_function_args(), first argument must be either function name or call reference");
      return NULL;
   }
   
   p1 = get_param(params, 1);

   args = dynamic_cast<QoreListNode *>(p1);
   if (!args && p1) 
   {
      args = new QoreListNode();
      // we borrow the reference for the new list
      args->push(p1);
   }

   class QoreNode *rv;
   if (p0->type == NT_STRING)
      rv = getProgram()->callFunction((reinterpret_cast<QoreStringNode *>(p0))->getBuffer(), args, xsink);
   else
      rv = p0->val.funcref->exec(args, xsink);

   if (p1 != args)
   {
      // we remove the element from the list without dereferencing
      args->shift();
      args->deref(xsink);
   }

   return rv;
}

static class QoreNode *f_existsFunction(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   // always return true if the argument is a call reference
   if (p0 && p0->type == NT_FUNCREF)
      return boolean_true();
   if (!p0 || p0->type != NT_STRING)
      return 0;

   QoreStringNode *str = reinterpret_cast<QoreStringNode *>(p0);
   if (getProgram()->existsFunction(str->getBuffer()))
      return boolean_true();
   if (builtinFunctions.find(str->getBuffer()))
      return boolean_true();
   return boolean_false();
}

// FIXME: should probably return constants
static class QoreNode *f_functionType(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;
   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   if (getProgram()->existsFunction(p0->getBuffer()))
      return new QoreStringNode("user");
   if (builtinFunctions.find(p0->getBuffer()))
      return new QoreStringNode("builtin");
   return NULL;
}

static class QoreNode *f_html_encode(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;
   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   class QoreStringNode *ns = new QoreStringNode(p0->getEncoding());
   ns->concatAndHTMLEncode(p0->getBuffer());
   return ns;
}

static class QoreNode *f_html_decode(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;
   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   QoreStringNode *ns = new QoreStringNode(p0->getEncoding());
   ns->concatAndHTMLDecode(p0);

   return ns;
}

static class QoreNode *f_get_default_encoding(const QoreListNode *params, ExceptionSink *xsink)
{
   return new QoreStringNode(QCS_DEFAULT->getCode());
}

class QoreNode *f_parse(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0, *p1;

   if (!(p0 = test_string_param(params, 0)) ||
       !(p1 = test_string_param(params, 1)))
      return 0;

   QoreProgram *pgm = getProgram();
   pgm->parse(p0, p1, xsink);
   return 0;
}

static class QoreNode *f_getClassName(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreObject *p0 = test_object_param(params, 0);
   if (!p0)
      return NULL;

   return new QoreStringNode(p0->getClass()->getName());
}

static class QoreNode *f_parseURL(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   QoreURL url(p0);
   if (url.isValid())
      return url.getHash();

   return NULL;
}

static class QoreNode *f_backquote(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   return backquoteEval(p0->getBuffer(), xsink);
}

static class QoreNode *f_makeBase64String(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   if (!p0)
      return 0;

   {
      const BinaryNode *b = dynamic_cast<const BinaryNode *>(p0);
      if (b)
	 return new QoreStringNode(b);
   }

   QoreStringNode *pstr = dynamic_cast<QoreStringNode *>(p0);
   if (!pstr)
      return 0;

   class QoreStringNode *str = new QoreStringNode();
   str->concatBase64(pstr);
   return str;
}

static class QoreNode *f_parseBase64String(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   return p0->parseBase64(xsink);
}

static class QoreNode *f_parseBase64StringToString(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   return p0->parseBase64ToString(xsink);
}

static class QoreNode *f_getModuleList(const QoreListNode *params, ExceptionSink *xsink)
{
   return MM.getModuleList();
}

static class QoreNode *f_getFeatureList(const QoreListNode *params, ExceptionSink *xsink)
{
   return getProgram()->getFeatureList();
}

static class QoreNode *f_hash_values(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreHashNode *p0 = test_hash_param(params, 0);
   if (!p0)
      return NULL;

   ReferenceHolder<QoreListNode> l(new QoreListNode(), xsink);
   class HashIterator hi(p0);
   while (hi.next() && !xsink->isEvent())
      l->push(hi.eval(xsink));

   if (*xsink)
      return NULL;

   return l.release();
}

void do_zlib_exception(int rc, char *func, class ExceptionSink *xsink)
{
   class QoreStringNode *desc = new QoreStringNode();
   desc->sprintf("%s(): ", func);
   switch (rc)
   {
      case Z_ERRNO:
	 desc->concat(strerror(errno));
	 break;

      case Z_STREAM_ERROR:
	 desc->concat("inconsistent stream state");
	 break;

      case Z_DATA_ERROR:
	 desc->set("unable to process input data; data corrupted");
	 break;

      case Z_MEM_ERROR:
	 desc->set("insufficient memory to complete operation");
	 break;

      case Z_BUF_ERROR:
	 desc->set("qore buffer-handling error (report as bug to qore developers)");
	 break;

      case Z_VERSION_ERROR:
	 desc->set("version mismatch on zlib shared library, check library requirements");
	 break;

      default:
	 desc->sprintf("error code %d encountered", rc);
	 break;
   }
   xsink->raiseException("ZLIB-ERROR", desc);
}

void do_deflate_end(z_stream *c_stream, class ExceptionSink *xsink)
{
   int rc = deflateEnd(c_stream);
   if (rc != Z_OK)
      do_zlib_exception(rc, "deflateEnd", xsink);
}

void do_inflate_end(z_stream *d_stream, class ExceptionSink *xsink)
{
   int rc = inflateEnd(d_stream);
   if (rc != Z_OK)
      do_zlib_exception(rc, "inflateEnd", xsink);
}

BinaryNode *qore_deflate(const void *ptr, unsigned long len, int level, ExceptionSink *xsink)
{
   z_stream c_stream; // compression stream
   c_stream.zalloc = Z_NULL;
   c_stream.zfree = Z_NULL;
   c_stream.opaque = Z_NULL;
   
   int rc = deflateInit(&c_stream, level);   
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "deflateInit", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_deflate_end, &c_stream, xsink);
   
   // allocate new buffer
   unsigned long bsize = len / 5 + 100;
   void *buf = malloc(bsize);
   
   c_stream.next_in = (Bytef *)ptr;
   c_stream.next_out = (Bytef *)buf;
   c_stream.avail_in = len;
   c_stream.avail_out = bsize;

   while (c_stream.avail_in)
   {
      rc = deflate(&c_stream, Z_NO_FLUSH);
      if (rc != Z_OK && rc != Z_BUF_ERROR)
      {
         free(buf);
         do_zlib_exception(rc, "deflate", xsink);
	 return NULL;
      }
      
      if (!c_stream.avail_out)
      {
         int new_space = ((len / 3) + 100);
         //printd(5, "deflate() Z_BUF_ERROR:1 bsize=%d->%d, new_space=%d avail_out=%d -> %d next_out=%08p\n", bsize, bsize + new_space, new_space, c_stream.avail_out, c_stream.avail_out + new_space, c_stream.next_out);
	 bsize += new_space;
	 c_stream.avail_out += new_space;
         buf = realloc(buf, bsize);
	 c_stream.next_out = ((Bytef *)buf) + c_stream.total_out;
      }
      //printd(5, "deflate() Z_BUF_ERROR:1 bsize=%d, avail_out=%d, next_out=%08p\n", bsize, c_stream.avail_out, c_stream.next_out);
   }

   while (true)
   {
      rc = deflate(&c_stream, Z_FINISH);
      if (rc == Z_STREAM_END)
         break;
      if (rc != Z_OK && rc != Z_BUF_ERROR)
      {
         free(buf);
         do_zlib_exception(rc, "deflate", xsink);
	 return NULL;
      }
      // resize buffer
      int new_space = 2; //((len / 3) + 100);
      //printd(5, "deflate() Z_BUF_ERROR:2 bsize=%d->%d, new_space=%d avail_out=%d -> %d, next_out=%08p\n", bsize, bsize + new_space, new_space, c_stream.avail_out, c_stream.avail_out + new_space, c_stream.next_out);
      
      bsize += new_space;
      c_stream.avail_out += new_space;
      buf = realloc(buf, bsize);
      c_stream.next_out = ((Bytef *)buf) + c_stream.total_out;
   }

   //printd(5, "deflate() buf=%08p, bsize=%d, avail_out=%d, size=%d, next_out=%08p\n", buf, bsize, c_stream.avail_out, bsize - c_stream.avail_out, c_stream.next_out);
   return new BinaryNode(buf, bsize - c_stream.avail_out);
}

class QoreStringNode *qore_inflate_to_string(BinaryNode *b, const QoreEncoding *enc, ExceptionSink *xsink)
{
   z_stream d_stream; // decompression stream
   d_stream.zalloc = Z_NULL;
   d_stream.zfree = Z_NULL;
   d_stream.opaque = Z_NULL;

   int rc = inflateInit(&d_stream);
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "inflateInit", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_inflate_end, &d_stream, xsink);
   
   int len = b->size();

   // allocate new buffer
   unsigned long bsize = len * 2 + 100;
   void *buf = malloc(bsize);
   
   d_stream.next_in = (Bytef *)b->getPtr();
   d_stream.next_out = (Bytef *)buf;
   d_stream.avail_in = b->size();
   d_stream.avail_out = bsize;

   while (true)
   {
      rc = inflate(&d_stream, Z_NO_FLUSH);
      if (rc == Z_STREAM_END)
	 break;
      if (rc == Z_BUF_ERROR)
      {
	 int new_space = ((len * 3) + 100);
	 bsize += new_space;
	 d_stream.avail_out += new_space;
	 buf = realloc(buf, bsize);
	 d_stream.next_out = ((Bytef *)buf) + d_stream.total_out;
      }
      else if (rc != Z_OK)
      {
	 free(buf);
	 do_zlib_exception(rc, "inflate", xsink);
	 return NULL;
      }
   }

   // how much data was decompressed
   len = bsize - d_stream.avail_out;

   // implement qppropriate QoreStringNode constructor so that a buffer can be taken and a null added if necessary
   class QoreStringNode *str = new QoreStringNode(enc);
   // add closing \0 if necessary
   if (((char *)buf)[len - 1])
      str->takeAndTerminate((char *)buf, len);
   else // otherwise take string and set length
      str->take((char *)buf, len - 1);

   return str;
}

BinaryNode *qore_inflate_to_binary(BinaryNode *b, class ExceptionSink *xsink)
{
   z_stream d_stream; // decompression stream
   d_stream.zalloc = Z_NULL;
   d_stream.zfree = Z_NULL;
   d_stream.opaque = Z_NULL;

   int rc = inflateInit(&d_stream);
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "inflateInit", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_inflate_end, &d_stream, xsink);
   
   int len = b->size();

   // allocate new buffer
   unsigned long bsize = len * 2 + 100;
   void *buf = malloc(bsize);
   
   d_stream.next_in = (Bytef *)b->getPtr();
   d_stream.next_out = (Bytef *)buf;
   d_stream.avail_in = b->size();
   d_stream.avail_out = bsize;

   while (true)
   {
      rc = inflate(&d_stream, Z_NO_FLUSH);
      if (rc == Z_STREAM_END)
	 break;
      if (rc == Z_BUF_ERROR)
      {
	 int new_space = ((len * 3) + 100);
	 bsize += new_space;
	 d_stream.avail_out += new_space;
	 buf = realloc(buf, bsize);
	 d_stream.next_out = ((Bytef *)buf) + d_stream.total_out;
      }
      else if (rc != Z_OK)
      {
	 free(buf);
	 do_zlib_exception(rc, "inflate", xsink);
	 return NULL;
      }
   }
   
   return new BinaryNode(buf, bsize - d_stream.avail_out);
}

BinaryNode *qore_gzip(const void *ptr, unsigned long len, int level, ExceptionSink *xsink)
{
   z_stream c_stream; // compression stream
   c_stream.zalloc = Z_NULL;
   c_stream.zfree = Z_NULL;
   c_stream.opaque = Z_NULL;
   c_stream.next_in = (Bytef *)ptr;
   c_stream.avail_in = len;

   int rc = deflateInit2(&c_stream, level, Z_DEFLATED, 31, 8,  Z_DEFAULT_STRATEGY);   
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "deflateInit2", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_deflate_end, &c_stream, xsink);
   
   // allocate new buffer
   unsigned long bsize = len / 5 + 100;
   void *buf = malloc(bsize);
   
   c_stream.next_out = (Bytef *)buf;
   c_stream.avail_out = bsize;

   while (c_stream.avail_in)
   {
      rc = deflate(&c_stream, Z_NO_FLUSH);
      if (rc != Z_OK && rc != Z_BUF_ERROR)
      {
	 free(buf);
	 do_zlib_exception(rc, "deflate", xsink);
	 return NULL;
      }

      if (!c_stream.avail_out)
      {
	 int new_space = ((len / 3) + 100);
	 //printd(5, "deflate() Z_BUF_ERROR:1 bsize=%d->%d, new_space=%d avail_out=%d -> %d next_out=%08p\n", bsize, bsize + new_space, new_space, c_stream.avail_out, c_stream.avail_out + new_space, c_stream.next_out);

	 bsize += new_space;
	 c_stream.avail_out += new_space;
	 buf = realloc(buf, bsize);
	 c_stream.next_out = ((Bytef *)buf) + c_stream.total_out;
      }
      //printd(5, "deflate() Z_BUF_ERROR:1 bsize=%d, avail_out=%d, next_out=%08p\n", bsize, c_stream.avail_out, c_stream.next_out);
   }

   while (true)
   {
      rc = deflate(&c_stream, Z_FINISH);
      if (rc == Z_STREAM_END)
	 break;
      if (rc != Z_OK && rc != Z_BUF_ERROR)
      {
	 free(buf);
	 do_zlib_exception(rc, "deflate", xsink);
	 return NULL;
      }
      // resize buffer
      int new_space = 2; //((len / 3) + 100);
      //printd(5, "deflate() Z_BUF_ERROR:2 bsize=%d->%d, new_space=%d avail_out=%d -> %d, next_out=%08p\n", bsize, bsize + new_space, new_space, c_stream.avail_out, c_stream.avail_out + new_space, c_stream.next_out);

      bsize += new_space;
      c_stream.avail_out += new_space;
      buf = realloc(buf, bsize);
      c_stream.next_out = ((Bytef *)buf) + c_stream.total_out;
   }

   //printd(5, "deflate() buf=%08p, bsize=%d, avail_out=%d, size=%d, next_out=%08p\n", buf, bsize, c_stream.avail_out, bsize - c_stream.avail_out, c_stream.next_out);
   return new BinaryNode(buf, bsize - c_stream.avail_out);
}

class QoreStringNode *qore_gunzip_to_string(BinaryNode *bin, const QoreEncoding *enc, ExceptionSink *xsink)
{
   z_stream d_stream; // decompression stream
   d_stream.zalloc = Z_NULL;
   d_stream.zfree = Z_NULL;
   d_stream.opaque = Z_NULL;

   d_stream.next_in = (Bytef *)bin->getPtr();
   d_stream.avail_in = bin->size();

   int rc = inflateInit2(&d_stream, 47);
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "inflateInit2", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_inflate_end, &d_stream, xsink);
   
   int len = bin->size();

   // allocate new buffer
   unsigned long bsize = len * 2 + 100;
   void *buf = malloc(bsize);
   
   d_stream.next_out = (Bytef *)buf;
   d_stream.avail_out = bsize;

   while (true)
   {
      rc = inflate(&d_stream, Z_NO_FLUSH);
      if (rc == Z_STREAM_END)
	 break;
      if (rc == Z_BUF_ERROR)
      {
	 int new_space = ((len * 3) + 100);
	 bsize += new_space;
	 d_stream.avail_out += new_space;
	 buf = realloc(buf, bsize);
	 d_stream.next_out = ((Bytef *)buf) + d_stream.total_out;
      }
      else if (rc != Z_OK)
      {
	 free(buf);
	 do_zlib_exception(rc, "inflate", xsink);
	 return NULL;
      }
   }

   // how much data was decompressed
   len = bsize - d_stream.avail_out;

   // implement qppropriate QoreStringNode constructor so that a buffer can be taken and a null added if necessary
   class QoreStringNode *str = new QoreStringNode(enc);
   // add closing \0 if necessary
   if (((char *)buf)[len - 1])
      str->takeAndTerminate((char *)buf, len);
   else // otherwise take string and set length
      str->take((char *)buf, len - 1);

   return str;
}

BinaryNode *qore_gunzip_to_binary(BinaryNode *bin, ExceptionSink *xsink)
{
   z_stream d_stream; // decompression stream
   d_stream.zalloc = Z_NULL;
   d_stream.zfree = Z_NULL;
   d_stream.opaque = Z_NULL;

   d_stream.next_in = (Bytef *)bin->getPtr();
   d_stream.avail_in = bin->size();

   int rc = inflateInit2(&d_stream, 47);
   if (rc != Z_OK)
   {
      do_zlib_exception(rc, "inflateInit2", xsink);
      return NULL;
   }

   ON_BLOCK_EXIT(do_inflate_end, &d_stream, xsink);
   
   int len = bin->size();

   // allocate new buffer
   unsigned long bsize = len * 2 + 100;
   void *buf = malloc(bsize);
   
   d_stream.next_out = (Bytef *)buf;
   d_stream.avail_out = bsize;

   while (true)
   {
      rc = inflate(&d_stream, Z_NO_FLUSH);
      if (rc == Z_STREAM_END)
	 break;
      if (rc == Z_BUF_ERROR)
      {
	 int new_space = ((len * 3) + 100);
	 bsize += new_space;
	 d_stream.avail_out += new_space;
	 buf = realloc(buf, bsize);
	 d_stream.next_out = ((Bytef *)buf) + d_stream.total_out;
      }
      else if (rc != Z_OK)
      {
	 free(buf);
	 do_zlib_exception(rc, "inflate", xsink);
	 return NULL;
      }
   }
   
   return new BinaryNode(buf, bsize - d_stream.avail_out);
}

static QoreNode *f_compress(const QoreListNode *params, ExceptionSink *xsink)
{
   // need a string or binary argument
   QoreNode *p0 = get_param(params, 0);
   if (is_nothing(p0))
      return NULL;

   QoreNode *p1 = get_param(params, 1);
   int level = p1 ? p1->getAsInt() : Z_DEFAULT_COMPRESSION;

   if (!level || level > 9)
   {
      xsink->raiseException("ZLIB-LEVEL-ERROR", "level must be between 0 - 9 (value passed: %d)", level);
      return NULL;
   }

   const void *ptr;
   unsigned long len;
   if (p0->type == NT_STRING)
   {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p0);
      ptr = str->getBuffer();
      len = str->strlen();
   }
   else if (p0->type == NT_BINARY)
   {
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p0);
      ptr = b->getPtr();
      len = b->size();
   }
   else
      return 0;

   if (!ptr || !len)
      return 0;

   return qore_deflate(ptr, len, level, xsink);
}

// syntax: uncompress_to_string(binary object, [encoding of new string])
static QoreNode *f_uncompress_to_string(const QoreListNode *params, ExceptionSink *xsink)
{
   // need binary argument
   BinaryNode *p0 = test_binary_param(params, 0);
   if (!p0)
      return NULL;

   const QoreEncoding *ccsid;
   QoreStringNode *p1 = test_string_param(params, 1);
   ccsid = p1 ? QEM.findCreate(p1) : QCS_DEFAULT;

   return qore_inflate_to_string(p0, ccsid, xsink);
}

// syntax: uncompress_to_binary(binary object)
static class QoreNode *f_uncompress_to_binary(const QoreListNode *params, ExceptionSink *xsink)
{
   // need binary argument
   BinaryNode *p0 = test_binary_param(params, 0);
   if (!p0)
      return NULL;

   return qore_inflate_to_binary(p0, xsink);
}

static class QoreNode *f_gzip(const QoreListNode *params, ExceptionSink *xsink)
{
   // need a string or binary argument
   QoreNode *p0 = get_param(params, 0);
   if (!p0)
      return NULL;

   QoreNode *p1 = get_param(params, 1);
   int level = p1 ? p1->getAsInt() : Z_DEFAULT_COMPRESSION;

   if (!level || level > 9)
   {
      xsink->raiseException("ZLIB-LEVEL-ERROR", "level must be between 1 - 9 (value passed: %d)", level);
      return NULL;
   }

   const void *ptr;
   unsigned long len;
   if (p0->type == NT_STRING)
   {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p0);
      ptr = str->getBuffer();
      len = str->strlen();
   }
   else if (p0->type == NT_BINARY)
   {
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p0);
      ptr = b->getPtr();
      len = b->size();
   }
   else
      return 0;

   if (!ptr || !len)
      return 0;

   return qore_gzip(ptr, len, level, xsink);
}

// syntax: gunzip_to_string(binary object, [encoding of new string])
static class QoreNode *f_gunzip_to_string(const QoreListNode *params, ExceptionSink *xsink)
{
   // need binary argument
   BinaryNode *p0 = test_binary_param(params, 0);
   if (!p0)
      return 0;

   const QoreEncoding *ccsid;
   QoreStringNode *p1 = test_string_param(params, 1);
   ccsid = p1 ? QEM.findCreate(p1) : QCS_DEFAULT;

   return qore_gunzip_to_string(p0, ccsid, xsink);
}

// syntax: gunzip_to_binary(binary object)
static class QoreNode *f_gunzip_to_binary(const QoreListNode *params, ExceptionSink *xsink)
{
   // need binary argument
   BinaryNode *p0 = test_binary_param(params, 0);
   if (!p0)
      return NULL;

   return qore_gunzip_to_binary(p0, xsink);
}

static class QoreNode *f_getByte(const QoreListNode *params, ExceptionSink *xsink)
{
   // need binary argument
   QoreNode *p0 = get_param(params, 0);
   if (!p0)
      return NULL;
   unsigned char *ptr;
   int size;
   if (p0->type == NT_BINARY)
   {
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p0);
      ptr = (unsigned char *)b->getPtr();
      size = b->size();
   }
   else if (p0->type == NT_STRING)
   {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p0);
      ptr = (unsigned char *)str->getBuffer();
      size = str->strlen();
   }
   else
      return NULL;

   QoreNode *p1 = get_param(params, 1);
   int offset = p1 ? p1->getAsInt() : 0;
   if (!ptr || offset >= size || offset < 0)
      return NULL;

   return new QoreBigIntNode(ptr[offset]);  
}

// same as splice operator, but operates on a copy of the list
static class QoreNode *f_splice(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   if (!p0)
      return NULL;

   QoreNode *p1, *p2;
   p1 = get_param(params, 1);
   p2 = get_param(params, 2);

   QoreStringNode *p3 = test_string_param(params, 3);
   int start = p1->getAsInt();

   {
      QoreStringNode *pstr = dynamic_cast<QoreStringNode *>(p0);
      if (pstr) {
	 TempQoreStringNode str(pstr->copy());

	 if (!p3)
	    if (is_nothing(p2))
	       str->splice(start, xsink);
	    else
	       str->splice(start, p2->getAsInt(), xsink);
	 else
	    str->splice(start, p2->getAsInt(), p3, xsink);
	 
	 return *xsink ? 0 : str.release();
      }
   }

   QoreListNode *lst = dynamic_cast<QoreListNode *>(p0);
   if (!lst)
      return 0;

   ReferenceHolder<QoreListNode> l(lst->copy(), xsink);

   if (!p3)
      if (is_nothing(p2))
	 l->splice(start, xsink);
      else
	 l->splice(start, p2->getAsInt(), xsink);
   else
      l->splice(start, p2->getAsInt(), p3, xsink);
   
   if (*xsink)
      return 0;

   return l.release();
}

static class QoreNode *f_makeHexString(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   if (!p0 || (p0->type != NT_BINARY && p0->type != NT_STRING))
      return NULL;

   class QoreStringNode *str = new QoreStringNode();
   if (p0->type == NT_STRING)
      str->concatHex(reinterpret_cast<const QoreStringNode *>(p0));
   else
      str->concatHex(reinterpret_cast<const BinaryNode *>(p0));
   return str;
}

static class QoreNode *f_parseHexString(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   return p0->parseHex(xsink);
}

// takes a hex string like "6d4f84e0" (with or without leading x or 0x) and returns the corresponding base-10 integer
static class QoreNode *f_hextoint(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   if (!p0->strlen())
      return zero();

   int64 rc = 0;
   int64 pow = 0;
   const char *buf = p0->getBuffer();
   if (*buf == '0' && *(buf + 1) == 'x')
      buf += 2;
   else if (*buf == 'x')
      buf++;
   for (const char *p = p0->strlen() + buf - 1; p >= buf; p--)
   {
      int n = get_nibble(*p, xsink);
      if (xsink->isException())
	 return NULL;
      if (!pow)
      {
	 rc = n;
	 pow = 16;
      }
      else
      {
	 rc += n * pow;
	 pow *= 16;
      }
   }
   return new QoreBigIntNode(rc);
}

// parses a string representing a number in a configurable base and returns the integer
static class QoreNode *f_strtoint(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;
   
   QoreNode *p1 = get_param(params, 1);
   int base = p1 ? p1->getAsInt() : 10;

   return new QoreBigIntNode(strtoll(p0->getBuffer(), NULL, base));
}

static class QoreNode *f_load_module(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0 = test_string_param(params, 0);
   if (!p0)
      return NULL;

   MM.runTimeLoadModule(p0->getBuffer(), xsink);
   return NULL;
}

static class QoreNode *f_set_signal_handler(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   int signal = p0 ? p0->getAsInt() : 0;
   if (!signal || signal > QORE_SIGNAL_MAX)
   {
      xsink->raiseException("SET-SIGNAL-HANDLER-ERROR", "%d is not a valid signal", signal);
      return NULL;
   }
   QoreNode *p1 = test_param(params, NT_FUNCREF, 1);
   if (!p1)
   {
      xsink->raiseException("SET-SIGNAL-HANDLER-ERROR", "expecting call reference as second argument to set_signal_handler()");
      return NULL;
   }
   QoreSignalManager::setHandler(signal, p1->val.funcref, xsink);
   return NULL;
}

static class QoreNode *f_remove_signal_handler(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreNode *p0 = get_param(params, 0);
   int signal = p0 ? p0->getAsInt() : 0;
   if (!signal || signal > QORE_SIGNAL_MAX)
   {
      xsink->raiseException("REMOVE-SIGNAL-HANDLER-ERROR", "%d is not a valid signal", signal);
      return NULL;
   }
   QoreSignalManager::removeHandler(signal, xsink);
   return NULL;
}

// returns a string with percent-encodings substituted for characters
static class QoreNode *f_decode_url(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p = test_string_param(params, 0);
   if (!p || !p->strlen())
      return 0;

   const char *c = p->getBuffer();
   QoreStringNode *str = new QoreStringNode(p->getEncoding());
   while (*c) {
      if (*c == '%' && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
	 char x[3] = { *(c + 1), *(c + 2), '\0' };
	 char code = strtol(x, 0, 16);
	 str->concat(code);
	 c += 3;
	 continue;
      }
      str->concat(*c);
      c++;
   }
   return str;
}

void init_misc_functions()
{
   // register builtin functions in this file
   builtinFunctions.add("parse", f_parse);
   builtinFunctions.add("call_function", f_call_function);
   builtinFunctions.add("call_function_args", f_call_function_args);
   builtinFunctions.add("existsFunction", f_existsFunction);
   builtinFunctions.add("functionType", f_functionType);
   builtinFunctions.add("html_encode", f_html_encode);
   builtinFunctions.add("html_decode", f_html_decode);
   builtinFunctions.add("get_default_encoding", f_get_default_encoding);
   builtinFunctions.add("parseURL", f_parseURL);
   builtinFunctions.add("getClassName", f_getClassName);
   builtinFunctions.add("backquote", f_backquote, QDOM_EXTERNAL_PROCESS);
   builtinFunctions.add("parseBase64String", f_parseBase64String);
   builtinFunctions.add("parseBase64StringToString", f_parseBase64StringToString);
   builtinFunctions.add("makeBase64String", f_makeBase64String);
   builtinFunctions.add("parseHexString", f_parseHexString);
   builtinFunctions.add("makeHexString", f_makeHexString);
   builtinFunctions.add("getModuleList", f_getModuleList);
   builtinFunctions.add("getFeatureList", f_getFeatureList);
   builtinFunctions.add("hash_values", f_hash_values);
   builtinFunctions.add("compress", f_compress);
   builtinFunctions.add("compress2", f_compress);
   builtinFunctions.add("uncompress_to_string", f_uncompress_to_string);
   builtinFunctions.add("uncompress_to_binary", f_uncompress_to_binary);
   builtinFunctions.add("gzip", f_gzip);
   builtinFunctions.add("gunzip_to_string", f_gunzip_to_string);
   builtinFunctions.add("gunzip_to_binary", f_gunzip_to_binary);
   builtinFunctions.add("getByte", f_getByte);
   builtinFunctions.add("splice", f_splice);
   builtinFunctions.add("hextoint", f_hextoint);
   builtinFunctions.add("strtoint", f_strtoint);
   builtinFunctions.add("load_module", f_load_module);
   builtinFunctions.add("set_signal_handler", f_set_signal_handler, QDOM_PROCESS);
   builtinFunctions.add("remove_signal_handler", f_remove_signal_handler, QDOM_PROCESS);
   builtinFunctions.add("decode_url", f_decode_url);
   
   // deprecated with stupid capitalization
   builtinFunctions.add("hexToInt", f_hextoint);
}

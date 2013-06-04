/*
  QoreSocket.cpp

  Socket Class for ipv4, ipv6 and UNIX domain sockets with SSL support
  
  Qore Programming Language

  Copyright 2003 - 2013 David Nichols

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

// FIXME: change int to qore_size_t where applicable! (ex: int rc = recv())

#include <qore/Qore.h>
#include <qore/QoreSocket.h>
#include <qore/intern/SSLSocketHelper.h>
#include <qore/intern/QC_Queue.h>

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef DEFAULT_SOCKET_BUFSIZE
#define DEFAULT_SOCKET_BUFSIZE 4096
#endif

#ifndef QORE_MAX_HEADER_SIZE
#define QORE_MAX_HEADER_SIZE 16384
#endif

static void concat_target(QoreString& str, const struct sockaddr *addr, const char* type = "target") {
   QoreString host;
   q_addr_to_string2(addr, host);
   if (!host.empty())
      str.sprintf(" (%s: %s:%d)", type, host.getBuffer(), q_get_port_from_addr(addr));
}

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
#define GETSOCKOPT_ARG_4 char*
#define SETSOCKOPT_ARG_4 const char*
#define SHUTDOWN_ARG SD_BOTH
#define QORE_INVALID_SOCKET ((int)INVALID_SOCKET)
#define QORE_SOCKET_ERROR SOCKET_ERROR

static int sock_get_raw_error() {
   return WSAGetLastError();
}

static int sock_get_error() {
   int rc = WSAGetLastError();

   switch (rc) {
      case 0:
	 errno = 0;
	 break;

      case WSANOTINITIALISED:
      case WSAEINVAL:
      case WSAENOTSOCK:
      case WSAEADDRNOTAVAIL:
      case WSAEAFNOSUPPORT:
      case WSAEOPNOTSUPP:
	 errno = EINVAL;
	 break;

      case WSAEADDRINUSE:
	 errno = EIO;
	 break;

      case WSAENETDOWN:
	 errno = ENODEV;
	 break;

      case WSAEFAULT:
	 errno = EFAULT;
	 break;

      case WSAENOBUFS:
	 errno = ENOMEM;
	 break;

      case WSAETIMEDOUT:
	 errno = ETIMEDOUT;
	 break;

      case WSAECONNREFUSED:
	 errno = ENOFILE;
	 break;

      case WSAEBADF:
	 errno = EBADF;
	 break;

#ifdef ECONNRESET
      case WSAECONNRESET:
	 errno = ECONNRESET;
	 break;
#endif

#ifdef DEBUG
      case WSAEALREADY:
      case WSAEINTR:
      case WSAEINPROGRESS:
      case WSAEWOULDBLOCK:
	 // should never get these here
	 printd(0, "sock_get_error() got unexpected error code %d; about to assert()\n", rc);
	 assert(false);
	 errno = EFAULT;
	 break;
#endif

      default:
	 printd(0, "sock_get_error() unknown code %d; about to assert()\n", rc);
	 assert(false);
	 errno = EFAULT;
	 break;
   }

   return errno;
}

static int check_windows_rc(int rc) {
   if (rc != SOCKET_ERROR)
      return 0;

   sock_get_error();
   return -1;
}

static void qore_socket_error_intern(int rc, ExceptionSink *xsink, const char *err, const char *cdesc, const char* mname = 0, const char* host = 0, const char* svc = 0, const struct sockaddr *addr = 0) {
   sock_get_error();
   if (!xsink)
      return;

   QoreStringNode* desc = new QoreStringNode;
   if (mname)
      desc->sprintf("error while executing Socket::%s(): ", mname);

   desc->concat(cdesc);

   if (addr) {
      assert(!host);
      assert(!svc);

      concat_target(*desc, addr);
   }
   else
      if (host && host[0]) {
         desc->sprintf(" (target: %s", host);
         if (svc)
            desc->sprintf(":%s", svc);
         desc->concat(")");
      }

   if (!errno) {
      xsink->raiseException(err, desc);
      return;
   }

   desc->concat(": ");
   char *buf;
   // get windows error message
   if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, rc, LANG_USER_DEFAULT, (LPTSTR)&buf, 0, 0)) {
      assert(!buf);
      desc->sprintf("Windows FormatMessage() failed on error code %d", rc);
   }

   assert(buf);
   desc->concat(buf);
   free(buf);

   xsink->raiseException(err, desc);
}

static void qore_socket_error(ExceptionSink *xsink, const char *err, const char *cdesc, const char* mname = 0, const char* host = 0, const char* svc = 0, const struct sockaddr *addr = 0) {
   qore_socket_error_intern(WSAGetLastError(), xsink, err, cdesc, mname, host, svc, addr);
}
#else
// UNIX/Cygwin
#define GETSOCKOPT_ARG_4 void*
#define SETSOCKOPT_ARG_4 void*
#define SHUTDOWN_ARG SHUT_RDWR
#define QORE_INVALID_SOCKET -1
#define QORE_SOCKET_ERROR -1

static int sock_get_raw_error() {
   return errno;
}

static int sock_get_error() {
   return errno;
}

static void qore_socket_error_intern(int rc, ExceptionSink *xsink, const char *err, const char *cdesc, const char* mname = 0, const char* host = 0, const char* svc = 0, const struct sockaddr *addr = 0) {
   assert(rc);
   if (!xsink)
      return;

   QoreStringNode* desc = new QoreStringNode;
   if (mname)
      desc->sprintf("error while executing Socket::%s(): ", mname);

   desc->concat(cdesc);

   if (addr) {
      assert(!host);
      assert(!svc);

      concat_target(*desc, addr);
   }
   else
      if (host) {
         desc->sprintf(" (target: %s", host);
         if (svc)
            desc->sprintf(":%s", svc);
         desc->concat(")");
      }

   xsink->raiseErrnoException(err, rc, desc);
}

static void qore_socket_error(ExceptionSink *xsink, const char *err, const char *cdesc, const char* mname = 0, const char* host = 0, const char* svc = 0, const struct sockaddr *addr = 0) {
   qore_socket_error_intern(errno, xsink, err, cdesc, mname, host, svc, addr);
}
#endif

int SSLSocketHelper::setIntern(const char* mname, int sd, X509* cert, EVP_PKEY *pk, ExceptionSink *xsink) {
   assert(!ssl);
   assert(!ctx);
   ctx = SSL_CTX_new(meth);
   if (!ctx) {
      sslError(xsink, mname, "SSL_CTX_new");
      return -1;
   }
   if (cert) {
      if (!SSL_CTX_use_certificate(ctx, cert)) {
	 sslError(xsink, mname, "SSL_CTX_use_certificate");
	 return -1;
      }
   }
   if (pk) {
      if (!SSL_CTX_use_PrivateKey(ctx, pk)) {
	 sslError(xsink, mname, "SSL_CTX_use_PrivateKey");
	 return -1;
      }
   }

   ssl = SSL_new(ctx);
   if (!ssl) {
      sslError(xsink, mname, "SSL_new");
      return -1;
   }

   // turn on SSL_MODE_ENABLE_PARTIAL_WRITE
   SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

   // turn on SSL_MODE_AUTO_RETRY for blocking I/O
   SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

   SSL_set_fd(ssl, sd);
   return 0;
}

int SSLSocketHelper::setClient(const char* mname, int sd, X509* cert, EVP_PKEY *pk, ExceptionSink *xsink) {
   meth = SSLv23_client_method();
   return setIntern(mname, sd, cert, pk, xsink);
}

int SSLSocketHelper::setServer(const char* mname, int sd, X509* cert, EVP_PKEY *pk, ExceptionSink *xsink) {
   meth = SSLv23_server_method();
   return setIntern(mname, sd, cert, pk, xsink);
}

// returns 0 for success
int SSLSocketHelper::connect(const char* mname, ExceptionSink *xsink) {
   if (SSL_connect(ssl) <= 0) {
      sslError(xsink, mname, "connect", "SSL_connect");
      return -1;
   }
   return 0;
}

// returns 0 for success
int SSLSocketHelper::accept(const char* mname, ExceptionSink *xsink) {
   int rc = SSL_accept(ssl);
   if (rc <= 0) {
      //printd(5, "SSLSocketHelper::accept() rc=%d\n", rc);
      sslError(xsink, mname, "SSL_accept");
      return -1;
   }
   return 0;
}

// returns 0 for success
int SSLSocketHelper::shutdown() {
   if (SSL_shutdown(ssl) < 0)
      return -1;
   return 0;
}

// returns 0 for success
int SSLSocketHelper::shutdown(ExceptionSink *xsink) {
   if (SSL_shutdown(ssl) < 0) {
      sslError(xsink, "shutdownSSL", "SSL_shutdown");
      return -1;
   }
   return 0;
}

// returns 0 for success
int SSLSocketHelper::write(const char* mname, const void* buf, int size, int timeout_ms, ExceptionSink* xsink) {
   return doSSLRW(mname, (void*)buf, size, timeout_ms, false, xsink);
}

const char *SSLSocketHelper::getCipherName() const {
   return SSL_get_cipher_name(ssl);
}

const char *SSLSocketHelper::getCipherVersion() const {
   return SSL_get_cipher_version(ssl);
}

X509 *SSLSocketHelper::getPeerCertificate() const {
   return SSL_get_peer_certificate(ssl);
}

long SSLSocketHelper::verifyPeerCertificate() const {	 
   X509 *cert = SSL_get_peer_certificate(ssl);
   
   if (!cert)
      return -1;
   
   long rc = SSL_get_verify_result(ssl);
   X509_free(cert);
   return rc;
}

struct qore_socketsource_private {
   QoreStringNode* address;
   QoreStringNode* hostname;
   
   DLLLOCAL qore_socketsource_private() : address(0), hostname(0) {
   }

   DLLLOCAL ~qore_socketsource_private() {
      if (address)  address->deref();
      if (hostname) hostname->deref();
   }

   DLLLOCAL void setAddress(QoreStringNode *addr) {
      assert(!address);
      address = addr;
   }

   DLLLOCAL void setAddress(const char *addr) {
      assert(!address);
      address = new QoreStringNode(addr);
   }

   DLLLOCAL void setHostName(const char *host) {
      assert(!hostname);
      hostname = new QoreStringNode(host);
   }

   DLLLOCAL void setAll(QoreObject *o, ExceptionSink *xsink) {
      if (address) {
	 o->setValue("source", address, xsink);
	 address = 0;
      }

      if (hostname) {
	 o->setValue("source_host", hostname, xsink);
	 hostname = 0;
      }
   }
};

SocketSource::SocketSource() : priv(new qore_socketsource_private) {
}

SocketSource::~SocketSource() {
   delete priv;
}

QoreStringNode *SocketSource::takeAddress() {
   QoreStringNode *addr = priv->address;
   priv->address = 0;
   return addr;
}

QoreStringNode *SocketSource::takeHostName() {
   QoreStringNode *host = priv->hostname;
   priv->hostname = 0;
   return host;
}

const char *SocketSource::getAddress() const {
   return priv->address ? priv->address->getBuffer() : 0;
}

const char *SocketSource::getHostName() const {
   return priv->hostname ? priv->hostname->getBuffer() : 0;
}

void SocketSource::setAll(QoreObject *o, ExceptionSink *xsink) {
   return priv->setAll(o, xsink);
}

static void se_not_open(const char* meth, ExceptionSink* xsink) {
   assert(xsink);
   xsink->raiseException("SOCKET-NOT-OPEN", "socket must be opened before Socket::%s() call", meth);
}

static void se_timeout(const char* meth, int timeout_ms, ExceptionSink* xsink) {
   assert(xsink);
   xsink->raiseException("SOCKET-TIMEOUT", "timed out after %d millisecond%s in Socket::%s() call", timeout_ms, timeout_ms == 1 ? "" : "s", meth);
}

static void se_closed(const char* mname, ExceptionSink *xsink) {
   xsink->raiseException("SOCKET-CLOSED", "error in Socket::%s(): remote end closed the connection", mname);
}

void QoreSocket::doException(int rc, const char *meth, int timeout_ms, ExceptionSink *xsink) {
   switch (rc) {
      case 0:
	 se_closed(meth, xsink);
	 break;
      case QSE_RECV_ERR: // recv() error
	 xsink->raiseException("SOCKET-RECV-ERROR", q_strerror(errno));
	 break;
      case QSE_NOT_OPEN:
	 se_not_open(meth, xsink);
	 break;
      case QSE_TIMEOUT:
	 se_timeout(meth, timeout_ms, xsink);
	 break;
      case QSE_SSL_ERR:
	 xsink->raiseException("SOCKET-SSL-ERROR", "SSL error in Socket::%s() call", meth);
	 break;
      default:
	 xsink->raiseException("SOCKET-ERROR", "unknown internal error code %d in Socket::%s() call", rc, meth);
	 break;
   }
}

struct qore_socket_private;
class OptionalNonBlockingHelper {
public:
   qore_socket_private& sock;
   ExceptionSink* xsink;
   bool set;

   DLLLOCAL OptionalNonBlockingHelper(qore_socket_private& s, bool n_set, ExceptionSink* xs);
   DLLLOCAL ~OptionalNonBlockingHelper();
};

struct qore_socket_private {
   int sock, sfamily, port, stype, sprot; //, sendTimeout, recvTimeout;
   const QoreEncoding* enc;
   bool del;
   std::string socketname;
   SSLSocketHelper* ssl;
   Queue* cb_queue;

   DLLLOCAL qore_socket_private(int n_sock = QORE_INVALID_SOCKET, int n_sfamily = AF_UNSPEC, int n_stype = SOCK_STREAM, int n_prot = 0, const QoreEncoding *n_enc = QCS_DEFAULT) : sock(n_sock), sfamily(n_sfamily), port(-1), stype(n_stype), sprot(n_prot), enc(n_enc), del(false), ssl(0), cb_queue(0) {
      //sendTimeout = recvTimeout = -1
   }

   DLLLOCAL ~qore_socket_private() {
      close_internal();

      // must be dereferenced and removed before deleting
      assert(!cb_queue);
   }

   DLLLOCAL int close() {
      int rc = close_internal();
      sfamily = AF_UNSPEC;
      stype = SOCK_STREAM;
      sprot = 0;

      return rc;
   }

   DLLLOCAL int close_internal() {
      //printd(5, "qore_socket_private::close_internal(this=%08p) sock=%d\n", this, sock);
      if (sock >= 0) {
	 // if an SSL connection has been established, shut it down first
	 if (ssl) {
	    ssl->shutdown();
	    delete ssl;
	    ssl = 0;
	 }

	 if (!socketname.empty()) {
	    if (del)
	       unlink(socketname.c_str());
	    socketname.clear();
	 }
	 del = false;
	 port = -1;
	 int rc;
	 while (true) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
	    rc = ::closesocket(sock);
#else
	    rc = ::close(sock);
#endif
	    // try again if close was interrupted by a signal
	    if (!rc || sock_get_error() != EINTR)
	       break;
	 }
	 //printd(5, "qore_socket_private::close_internal(this=%08p) close(%d) returned %d\n", this, sock, rc);
	 do_close_event();
	 sock = QORE_INVALID_SOCKET;
	 return rc;
      }
      else
	 return 0; 
   }

   DLLLOCAL int getSendTimeout() const {
      struct timeval tv;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
      // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
      // but the library expects a 32-bit value
      int size = sizeof(struct timeval);
#else
      socklen_t size = sizeof(struct timeval);
#endif

      if (getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (GETSOCKOPT_ARG_4)&tv, (socklen_t *)&size))
	 return -1;

      return tv.tv_sec * 1000 + tv.tv_usec / 1000;
   }

   DLLLOCAL int getRecvTimeout() const {
      struct timeval tv;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
      // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
      // but the library expects a 32-bit value
      int size = sizeof(struct timeval);
#else
      socklen_t size = sizeof(struct timeval);
#endif

      if (getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (GETSOCKOPT_ARG_4)&tv, (socklen_t *)&size))
	 return -1;

      return tv.tv_sec * 1000 + tv.tv_usec / 1000;
   }

   DLLLOCAL int getPort() {
      // if we don't need to find out what port we are, then return current value
      if (sock == QORE_INVALID_SOCKET || (sfamily != AF_INET && sfamily != AF_INET6) || port > 0)
	 return port;

      // otherwise find out what port we're connected to
      struct sockaddr_storage addr;
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
      // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
      int size = sizeof addr;
#else
      socklen_t size = sizeof addr;
#endif

      if (getsockname(sock, (struct sockaddr *)&addr, (socklen_t *)&size) < 0)
	 return -1;

      port = q_get_port_from_addr((const struct sockaddr *)&addr);
      return port;
   }

   DLLLOCAL static void do_header(const char *key, QoreString &hdr, const AbstractQoreNode *v) {
      switch (v->getType()) {
	 case NT_STRING:
	    hdr.sprintf("%s: %s\r\n", key, reinterpret_cast<const QoreStringNode*>(v)->getBuffer());
	    break;
	 case NT_INT:
	    hdr.sprintf("%s: "QLLD"\r\n", key, reinterpret_cast<const QoreBigIntNode*>(v)->val);
	    break;
	 case NT_FLOAT:
	    hdr.sprintf("%s: %f\r\n", key, reinterpret_cast<const QoreFloatNode*>(v)->f);
	    break;
	 case NT_NUMBER:
	    hdr.sprintf("%s: ", key);
	    reinterpret_cast<const QoreNumberNode*>(v)->toString(hdr);
	    hdr.concat("\r\n"); 
	    break;
	 case NT_BOOLEAN:
	    hdr.sprintf("%s: %d\r\n", key, reinterpret_cast<const QoreBoolNode*>(v)->getValue());
	    break;
      }
   }

   DLLLOCAL static void do_headers(QoreString &hdr, const QoreHashNode *headers, qore_size_t size, bool addsize = false) {
      // RFC-2616 4.4 (http://tools.ietf.org/html/rfc2616#section-4.4)
      // add Content-Length: 0 to headers for responses without a body where there is no transfer-encoding
      if (headers) {
	 ConstHashIterator hi(headers);

	 while (hi.next()) {
	    const AbstractQoreNode *v = hi.getValue();
	    const char* key = hi.getKey();
	    if (addsize && !strcasecmp(key, "transfer-encoding"))
	       addsize = false;
	    if (v && v->getType() == NT_LIST) {
	       ConstListIterator li(reinterpret_cast<const QoreListNode *>(v));
	       while (li.next())
		  do_header(key, hdr, li.getValue());
	    }
	    else
	       do_header(key, hdr, hi.getValue());
	 }
      }
      // add data and content-length header if necessary
      if (size || addsize)
	 hdr.sprintf("Content-Length: "QSD"\r\n", size);

      hdr.concat("\r\n");
   }

   DLLLOCAL static void *get_in_addr(struct sockaddr *sa) {
      if (sa->sa_family == AF_INET)
	 return &(((struct sockaddr_in *)sa)->sin_addr);
      return &(((struct sockaddr_in6 *)sa)->sin6_addr);
   }

   DLLLOCAL static size_t get_in_len(struct sockaddr *sa) {
      if (sa->sa_family == AF_INET)
	 return sizeof(struct sockaddr_in);
      return sizeof(struct sockaddr_in6);
   }

   DLLLOCAL int listen() {
      if (sock == QORE_INVALID_SOCKET)
	 return QSE_NOT_OPEN;
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
      if (::listen(sock, 5)) {
	 // set errno
	 sock_get_error();
	 return -1;
      }
      return 0;
#else
      return ::listen(sock, 5);
#endif
   }

   DLLLOCAL int accept_intern(struct sockaddr *addr, socklen_t *size, int timeout_ms = -1, ExceptionSink *xsink = 0) {
      while (true) {
	 if (timeout_ms >= 0 && !isDataAvailable(timeout_ms, "accept", xsink)) {
	    if (xsink && *xsink)
	       return -1;
	    // do not throw exception here, NOTHING will be returned in Qore on timeout
	    return QSE_TIMEOUT; // -3
	 }

	 int rc = ::accept(sock, addr, size);
	 if (rc != QORE_INVALID_SOCKET)
	    return rc;

	 // retry if interrupted by a signal
	 if (sock_get_error() == EINTR)
	    continue;

	 qore_socket_error(xsink, "SOCKET-ACCEPT-ERROR", "error in accept()", 0, 0, 0, addr);
	 return -1;
      }
   }
   
   // returns a new socket
   DLLLOCAL int accept_internal(SocketSource *source, int timeout_ms = -1, ExceptionSink *xsink = 0) {
      if (sock == QORE_INVALID_SOCKET) {
	 if (xsink)
	    xsink->raiseException("SOCKET-NOT-OPEN", "socket must be opened, bound, and in a listening state before new connections can be accepted");
	 return QSE_NOT_OPEN;
      }

      int rc;
      if (sfamily == AF_UNIX) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
	 if (xsink)
	    xsink->raiseException("SOCKET-ACCEPT-ERROR", "UNIX sockets are not available under Windows");
	 return -1;
#else
	 struct sockaddr_un addr_un;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
	 // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
	 // but the library expects a 32-bit value
	 int size = sizeof(struct sockaddr_un);
#else
	 socklen_t size = sizeof(struct sockaddr_un);
#endif
	 rc = accept_intern((struct sockaddr *)&addr_un, (socklen_t *)&size, timeout_ms, xsink);
	 //printd(1, "qore_socket_private::accept_internal() "QSD" bytes returned\n", size);

	 if (rc >= 0 && source) {
	    QoreStringNode *addr = new QoreStringNode(enc);
	    addr->sprintf("UNIX socket: %s", socketname.c_str());
	    source->priv->setAddress(addr);
	    source->priv->setHostName("localhost");
	 }
#endif // windows
      }
      else if (sfamily == AF_INET || sfamily == AF_INET6) {
	 struct sockaddr_storage addr_in;
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
	 // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
	 // but the library expects a 32-bit value
	 int size = sizeof(addr_in);
#else
	 socklen_t size = sizeof(addr_in);
#endif

	 rc = accept_intern((struct sockaddr *)&addr_in, (socklen_t *)&size, timeout_ms, xsink);
	 //printd(1, "qore_socket_private::accept_internal() rc=%d, %d bytes returned\n", rc, size);

	 if (rc >= 0 && source) {
	    char host[NI_MAXHOST + 1];
	    char service[NI_MAXSERV + 1];

	    if (!getnameinfo((struct sockaddr *)&addr_in, get_in_len((struct sockaddr *)&addr_in), host, sizeof(host), service, sizeof(service), NI_NUMERICSERV)) {
	       source->priv->setHostName(host);
	    }

	    // get ipv4 or ipv6 address
	    char ifname[INET6_ADDRSTRLEN];
	    if (inet_ntop(addr_in.ss_family, get_in_addr((struct sockaddr *)&addr_in), ifname, sizeof(ifname))) {
	       //printd(5, "inet_ntop() '%s' host: '%s'\n", ifname, host);
	       source->priv->setAddress(ifname);
	    }
	 }
      }
      else {
	 // should not happen
	 if (xsink)
	    xsink->raiseException("SOCKET-ACCEPT-ERROR", "do not know how to accept connections with address family %d", sfamily);
	 rc = -1;
      }
      return rc;
   }

   DLLLOCAL void cleanup(ExceptionSink *xsink) {
      if (cb_queue) {
	 // close the socket before the delete message is put on the queue
	 // the socket would be closed anyway in the destructor
	 close_internal();

	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_DELETED), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 cb_queue->pushAndTakeRef(h);

	 // deref and remove event queue
	 cb_queue->deref(xsink);
	 cb_queue = 0;
      }
   }

   DLLLOCAL void setEventQueue(Queue *cbq, ExceptionSink *xsink) {
      if (cb_queue)
	 cb_queue->deref(xsink);
      cb_queue = cbq;
   }

   DLLLOCAL void do_start_ssl_event() {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_START_SSL), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_ssl_established_event() {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_SSL_ESTABLISHED), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 h->setKeyValue("cipher", new QoreStringNode(ssl->getCipherName()), 0);
	 h->setKeyValue("cipher_version", new QoreStringNode(ssl->getCipherVersion()), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_connect_event(int af, const struct sockaddr* addr, const char *target, const char *service = 0, int prt = -1) {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_CONNECTING), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
         QoreStringNode *str = q_addr_to_string2(addr);
          if (str)
             h->setKeyValue("address", str, 0);
          else
             h->setKeyValue("error", q_strerror(sock_get_error()), 0);
         q_af_to_hash(af, *h, 0);
	 h->setKeyValue("target", new QoreStringNode(target), 0);
	 if (service)
	    h->setKeyValue("service", new QoreStringNode(service), 0);
	 if (prt != -1)
	    h->setKeyValue("port", new QoreBigIntNode(prt), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_connected_event() {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_CONNECTED), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_chunked_read(int event, qore_size_t bytes, qore_size_t total_read, int source) {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(event), 0);
	 h->setKeyValue("source", new QoreBigIntNode(source), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 if (event == QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED)
	    h->setKeyValue("read", new QoreBigIntNode(bytes), 0);
	 else
	    h->setKeyValue("size", new QoreBigIntNode(bytes), 0);
	 h->setKeyValue("total_read", new QoreBigIntNode(total_read), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_read_http_header(int event, const QoreHashNode *headers, int source) {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(event), 0);
	 h->setKeyValue("source", new QoreBigIntNode(source), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 h->setKeyValue("headers", headers->hashRefSelf(), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_send_http_message(const QoreString &str, const QoreHashNode *headers, int source) {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_HTTP_SEND_MESSAGE), 0);
	 h->setKeyValue("source", new QoreBigIntNode(source), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 h->setKeyValue("message", new QoreStringNode(str), 0);
	 //printd(0, "do_send_http_message() str='%s' headers=%p (%d %s)\n", str.getBuffer(), headers, headers->getType(), headers->getTypeName());
	 h->setKeyValue("headers", headers->hashRefSelf(), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_close_event() {
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_CHANNEL_CLOSED), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_read_event(qore_size_t bytes_read, qore_size_t total_read, qore_size_t bufsize = 0) {
      // post bytes read on event queue, if any
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_PACKET_READ), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 h->setKeyValue("read", new QoreBigIntNode(bytes_read), 0);
	 h->setKeyValue("total_read", new QoreBigIntNode(total_read), 0);
	 // set total bytes to read and remaining bytes if bufsize > 0
	 if (bufsize > 0)
	    h->setKeyValue("total_to_read", new QoreBigIntNode(bufsize), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_send_event(int bytes_sent, int total_sent, int bufsize) {
      // post bytes sent on event queue, if any
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_PACKET_SENT), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 h->setKeyValue("sent", new QoreBigIntNode(bytes_sent), 0);
	 h->setKeyValue("total_sent", new QoreBigIntNode(total_sent), 0);
	 h->setKeyValue("total_to_send", new QoreBigIntNode(bufsize), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_resolve_event(const char *host, const char *service = 0) {
      // post bytes sent on event queue, if any
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_HOSTNAME_LOOKUP), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 if (host)
	    h->setKeyValue("name", new QoreStringNode(host), 0);
	 if (service)
	    h->setKeyValue("service", new QoreStringNode(service), 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL void do_resolved_event(const struct sockaddr *addr) {
      // post bytes sent on event queue, if any
      if (cb_queue) {
	 QoreHashNode *h = new QoreHashNode;
	 h->setKeyValue("event", new QoreBigIntNode(QORE_EVENT_HOSTNAME_RESOLVED), 0);
	 h->setKeyValue("source", new QoreBigIntNode(QORE_SOURCE_SOCKET), 0);
	 h->setKeyValue("id", new QoreBigIntNode((int64)this), 0);
	 QoreStringNode *str = q_addr_to_string2(addr);
	 if (str)
	    h->setKeyValue("address", str, 0);
	 else
	    h->setKeyValue("error", q_strerror(sock_get_error()), 0);
	 int prt = q_get_port_from_addr(addr);
	 if (prt > 0)
	    h->setKeyValue("port", new QoreBigIntNode(prt), 0);
	 q_af_to_hash(addr->sa_family, *h, 0);
	 cb_queue->pushAndTakeRef(h);
      }
   }

   DLLLOCAL int64 getObjectIDForEvents() const {
      return (int64)this;
   }

   DLLLOCAL int connectUNIX(const char *p, int sock_type, int protocol, ExceptionSink *xsink) {
      QORE_TRACE("connectUNIX()");

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
      xsink->raiseException("SOCKET-CONNECTUNIX-ERROR", "UNIX sockets are not available under Windows");
      return -1;
#else
      // close socket if already open
      close();

      printd(5, "qore_socket_private::connectUNIX(%s)\n", p);
	 
      struct sockaddr_un addr;
	 
      addr.sun_family = AF_UNIX;
      // copy path and terminate if necessary
      strncpy(addr.sun_path, p, sizeof(addr.sun_path) - 1);
      addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
      if ((sock = socket(AF_UNIX, sock_type, protocol)) == QORE_SOCKET_ERROR) {
	 if (xsink)
	    xsink->raiseException("SOCKET-CONNECT-ERROR", q_strerror(errno));
	    
	 return -1;
      }
	 
      do_connect_event(AF_UNIX, (sockaddr*)&addr, p);
      while (true) {
	 if (!::connect(sock, (const sockaddr *)&addr, sizeof(struct sockaddr_un)))
	    break;

	 // try again if we were interrupted by a signal
	 if (sock_get_error() == EINTR)
	    continue;

	 // otherwise close the socket and return an exception with the error code
	 // do not have to worry about windows API calls here; this is a UNIX-only function
	 ::close(sock);

	 sock = QORE_INVALID_SOCKET;
	 qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, p);
	    
	 return -1;	    
      }

      // save file name for deleting when socket is closed
      socketname = addr.sun_path;
      sfamily = AF_UNIX;
	 
      do_connected_event();

      return 0;
#endif // windows
   }

   // socket must be open!
   DLLLOCAL int select(int timeout_ms, bool read, const char* mname, ExceptionSink* xsink) {
      if (sock == QORE_INVALID_SOCKET) {
	 if (xsink)
	    se_not_open(mname, xsink);
	 return -1;
      }

      assert(sock != QORE_INVALID_SOCKET);

      fd_set sfs;
      
      FD_ZERO(&sfs);
      FD_SET(sock, &sfs);

      struct timeval tv;
      int rc;
      while (true) {
	 tv.tv_sec  = timeout_ms / 1000;
	 tv.tv_usec = (timeout_ms % 1000) * 1000;

	 rc = read ? ::select(sock + 1, &sfs, 0, 0, &tv) : ::select(sock + 1, 0, &sfs, 0, &tv);
	 if (rc != QORE_SOCKET_ERROR || sock_get_error() != EINTR)
	    break;
      }
#ifdef EBADF
      // mark the socket as closed if the select call fails due to a bad file descriptor error
      if (rc && sock_get_error() == EBADF) {
	 close();
	 if (xsink)
	    se_closed(mname, xsink);
      }
#endif

      return rc;
   }

   DLLLOCAL bool isDataAvailable(int timeout_ms, const char* mname, ExceptionSink* xsink) {
      return select(timeout_ms, true, mname, xsink);
   }

   DLLLOCAL bool isWriteFinished(int timeout_ms, const char* mname, ExceptionSink* xsink) {
      return select(timeout_ms, false, mname, xsink);
   }

   DLLLOCAL int close_and_exit() {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
      closesocket(sock);
#else
      ::close(sock);
#endif
      sock = QORE_INVALID_SOCKET;
      return -1;
   }

   DLLLOCAL int connectINETTimeout(int timeout_ms, const struct sockaddr *ai_addr, qore_size_t ai_addrlen, ExceptionSink *xsink, bool only_timeout) {
      while (true) {
	 if (!::connect(sock, ai_addr, ai_addrlen))
	    return 0;

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
	 if (WSAGetLastError() != WSAEWOULDBLOCK) {
	    qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, 0, 0, ai_addr);
	    break;
	 }
#else
	 // try again if we were interrupted by a signal
	 if (errno == EINTR)
	    continue;

	 if (errno != EINPROGRESS)
	    break;
#endif

	 //printd(5, "qore_socket_private::connectINETTimeout() errno=%d\n", errno);

	 // check for timeout or connection with EINPROGRESS
	 while (true) {
	    int rc = select(timeout_ms, false, "connectINETTimeout", xsink);
	    if (xsink && *xsink)
	       return -1;

	    //printd(0, "select(%d) returned %d\n", timeout_ms, rc);
	    if (rc == QORE_SOCKET_ERROR && sock_get_error() != EINTR) { 
	       if (xsink && !only_timeout)
		  qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in select() with Socket::connect() with timeout", 0, 0, 0, ai_addr);
	       return -1;
	    } 
	    else if (rc > 0) { 
	       // socket selected for write 
	       socklen_t lon = sizeof(int);
	       int val;

	       if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (GETSOCKOPT_ARG_4)(&val), &lon) == QORE_SOCKET_ERROR) { 
		  if (xsink && !only_timeout)
		     qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in getsockopt()", 0, 0, 0, ai_addr);
		  return -1;
	       } 
	       
	       if (val) {
		  if (only_timeout) {
		     errno = val;
		     return -1;
		  }
		  qore_socket_error_intern(val, xsink, "SOCKET-CONNECT-ERROR", "error in getsockopt()", 0, 0, 0, ai_addr);
		  return -1;
	       }

	       // connected successfully within the timeout period
	       return 0;
	    }
	    else { 
	       if (xsink) {
	          QoreStringNode* desc = new QoreStringNodeMaker("timeout in connection after %dms", timeout_ms);
	          concat_target(*desc, ai_addr);
	          xsink->raiseException("SOCKET-CONNECT-ERROR", desc);
	       }
	       return -1;
	    }
	 }
      }

      return -1;
   }

   DLLLOCAL int sock_errno_err(const char *err, const char *desc, ExceptionSink *xsink) {
      sock = QORE_INVALID_SOCKET;
      qore_socket_error(xsink, err, desc);
      return -1;
   }

   DLLLOCAL int set_non_blocking(bool non_blocking, ExceptionSink *xsink = 0) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
      u_long mode = non_blocking ? 1 : 0;
      int rc = ioctlsocket(sock, FIONBIO, &mode);
      if (check_windows_rc(rc))
	 return sock_errno_err("SOCKET-CONNECT-ERROR", "error in ioctlsocket(FIONBIO)", xsink);
#else
      int arg;

      // get socket descriptor status flags
      if ((arg = fcntl(sock, F_GETFL, 0)) < 0)
	 return sock_errno_err("SOCKET-CONNECT-ERROR", "error in fcntl() getting socket descriptor status flag", xsink);

      if (non_blocking) // set non-blocking
	 arg |= O_NONBLOCK; 
      else // set blocking
	 arg &= ~O_NONBLOCK;

      if (fcntl(sock, F_SETFL, arg) < 0)
	 return sock_errno_err("SOCKET-CONNECT-ERROR", "error in fcntl() setting socket descriptor status flag", xsink);
#endif

      return 0;
   }

   DLLLOCAL int connectINET(const char *host, const char *service, int timeout_ms, ExceptionSink *xsink = 0, int family = AF_UNSPEC, int type = SOCK_STREAM, int protocol = 0) {
      family = q_get_af(family);
      type = q_get_sock_type(type);

      QORE_TRACE("qore_socket_private::connectINET()");

      // close socket if already open
      close();

      printd(5, "qore_socket_private::connectINET(%s:%s, %dms)\n", host, service, timeout_ms);

      do_resolve_event(host, service);

      QoreAddrInfo ai;
      if (ai.getInfo(xsink, host, service, family, 0, type, protocol))
	 return -1;

      struct addrinfo *aip = ai.getAddrInfo();

      // emit all "resolved" events
      if (cb_queue)
	 for (struct addrinfo *p = aip; p; p = p->ai_next)
	    do_resolved_event(p->ai_addr);

      int prt = q_get_port_from_addr(aip->ai_addr);

      for (struct addrinfo *p = aip; p; p = p->ai_next) {
	 if (!connectINETIntern(host, service, p->ai_family, p->ai_addr, p->ai_addrlen, p->ai_socktype, p->ai_protocol, prt, timeout_ms, xsink, true))
	    return 0;
	 if (xsink && *xsink)
	    break;
      }

      if (xsink && !*xsink)
	 qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, host, service);
      return -1;
   }

   DLLLOCAL int connectINETIntern(const char *host, const char *service, int ai_family, struct sockaddr *ai_addr, size_t ai_addrlen, int ai_socktype, int ai_protocol, int prt, int timeout_ms, ExceptionSink *xsink, bool only_timeout = false) {
      //printd(5, "qore_socket_private::connectINETIntern() host=%s service=%s family=%d\n", host, service, ai_family);
      if ((sock = socket(ai_family, ai_socktype, ai_protocol)) == QORE_INVALID_SOCKET) {
	 if (xsink)
	    xsink->raiseErrnoException("SOCKET-CONNECT-ERROR", errno, "cannot establish a connection to %s:%s", host, service);

	 return -1;
      }

      //printd(5, "qore_socket_private::connectINETIntern(this=%08p, host='%s', port=%d, timeout_ms=%d) sock=%d\n", this, host, port, timeout_ms, sock);

      int rc;

      // perform connect with timeout if a non-negative timeout was passed
      if (timeout_ms >= 0) {
	 // set non-blocking
	 if (set_non_blocking(true, xsink)) {
	    close_and_exit();
	    return -1;
	 }

	 do_connect_event(ai_family, ai_addr, host, service, prt);

	 rc = connectINETTimeout(timeout_ms, ai_addr, ai_addrlen, xsink, only_timeout);
	 //printd(5, "qore_socket_private::connectINETIntern() errno=%d rc=%d, xsink=%d\n", errno, rc, xsink && *xsink);

	 // set blocking
	 if (set_non_blocking(false, xsink)) {
	    close_and_exit();
	    return -1;
	 }
      }
      else {
	 do_connect_event(ai_family, ai_addr, host, service, prt);

	 while (true) {
	    rc = ::connect(sock, ai_addr, ai_addrlen);
	    // try again if rc == -1 and errno == EINTR
	    if (!rc || sock_get_error() != EINTR)
	       break;
	 }
      }

      if (rc < 0) {
	 if (xsink && (!only_timeout || errno == ETIMEDOUT))
	    qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, host, service);

	 return close_and_exit();
      }

      sfamily = ai_family;
      stype = ai_socktype;
      sprot = ai_protocol;
      port = prt;
      //printd(5, "qore_socket_private::connectINETIntern(this=%08p, host='%s', port=%d, timeout_ms=%d) success, rc=%d, sock=%d\n", this, host, port, timeout_ms, rc, sock);

      do_connected_event();
      return 0;
   }

   DLLLOCAL int upgradeClientToSSLIntern(const char* mname, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
      assert(!ssl);
      ssl = new SSLSocketHelper(*this);
      int rc;
      do_start_ssl_event();
      if ((rc = ssl->setClient(mname, sock, cert, pkey, xsink)) || ssl->connect(mname, xsink)) {
	 delete ssl;
	 ssl = 0;
	 return rc;
      }
      do_ssl_established_event();
      return 0;
   }

   DLLLOCAL int upgradeServerToSSLIntern(const char* mname, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
      assert(!ssl);
      ssl = new SSLSocketHelper(*this);
      do_start_ssl_event();
      if (ssl->setServer(mname, sock, cert, pkey, xsink) || ssl->accept(mname, xsink)) {
	 delete ssl;
	 ssl = 0;
	 return -1;
      }
      do_ssl_established_event();
      return 0;
   }

   // returns 0 = success, -1 = error
   DLLLOCAL int openUNIX(int sock_type = SOCK_STREAM, int protocol = 0) {
      if (sock != QORE_INVALID_SOCKET)
	 close();

      if ((sock = socket(AF_UNIX, sock_type, protocol)) == QORE_INVALID_SOCKET) {
	 return -1;
      }

      sfamily = AF_UNIX;
      stype = sock_type;
      sprot = protocol;
      port = -1;
      return 0;
   }

   // returns 0 = success, -1 = error
   DLLLOCAL int openINET(int family = AF_INET, int sock_type = SOCK_STREAM, int protocol = 0) {
      if (sock != QORE_INVALID_SOCKET)
	 close();

      if ((sock = socket(family, sock_type, protocol)) == QORE_INVALID_SOCKET)
	 return -1;

      sfamily = family;
      stype = sock_type;
      sprot = protocol;
      port = -1;
      return 0;
   }

   DLLLOCAL int reuse(int opt) {
      //printf("qore_socket_private::reuse(%s)\n", opt ? "true" : "false");
      return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (SETSOCKOPT_ARG_4)&opt, sizeof(int));
   }

   DLLLOCAL int bindIntern(struct sockaddr *ai_addr, size_t ai_addrlen, int prt, bool reuseaddr, ExceptionSink *xsink = 0) {
      reuse(reuseaddr);

      if ((::bind(sock, ai_addr, ai_addrlen)) == QORE_SOCKET_ERROR) {
	 qore_socket_error(xsink, "SOCKET-BIND-ERROR", "error in bind()", 0, 0, 0, ai_addr);
	 close();
	 return -1;
      }

      // set port number
      if (prt)
	 port = prt;
      else {
	 // get port number
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
	 // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
	 int len = ai_addrlen;
#else
	 socklen_t len = ai_addrlen;
#endif

	 if (getsockname(sock, ai_addr, &len))
	    port = -1;
	 else
	    port = q_get_port_from_addr(ai_addr);
      }
      return 0;
   }

   // bind to UNIX domain socket file
   DLLLOCAL int bindUNIX(const char *name, int socktype = SOCK_STREAM, int protocol = 0, ExceptionSink *xsink = 0) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
      xsink->raiseException("SOCKET-BINDUNIX-ERROR", "UNIX sockets are not available under Windows");
      return -1;
#else
      close();

      // try to open socket if necessary
      if (openUNIX(socktype, protocol)) {
	 if (xsink)
	    xsink->raiseErrnoException("SOCKET-BIND-ERROR", errno, "error opening UNIX socket ('%s') for bind", name);
	 return -1;
      }

      struct sockaddr_un addr;
      addr.sun_family = AF_UNIX;
      // copy path and terminate if necessary
      strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);
      addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

      if (bindIntern((sockaddr *)&addr, sizeof(struct sockaddr_un), -1, false, xsink))
	 return -1;

      // save socket file name for deleting on close
      socketname = addr.sun_path;
      // delete UNIX domain socket on close
      del = true;
      return 0;
#endif // windows
   }

   DLLLOCAL int bindINET(const char *name, const char *service, bool reuseaddr = true, int family = AF_UNSPEC, int socktype = SOCK_STREAM, int protocol = 0, ExceptionSink *xsink = 0) {
      family = q_get_af(family);
      socktype = q_get_sock_type(socktype);

      close();

      QoreAddrInfo ai;
      do_resolve_event(name, service);
      if (ai.getInfo(xsink, name, service, family, AI_PASSIVE, socktype, protocol))
	 return -1;

      struct addrinfo *aip = ai.getAddrInfo();
      // first emit all "resolved" events
      if (cb_queue)
	 for (struct addrinfo *p = aip; p; p = p->ai_next)
	    do_resolved_event(p->ai_addr);

      // try to open socket if necessary
      if (openINET(aip->ai_family, aip->ai_socktype, protocol)) {
	 qore_socket_error(xsink, "SOCKET-BINDINET-ERROR", "error opening socket for bind", 0, name, service);
	 return -1;
      }

      int prt = q_get_port_from_addr(aip->ai_addr);

      int en = 0;
      // iterate through addresses and bind to the first interface possible
      for (struct addrinfo *p = aip; p; p = p->ai_next) {
	 if (!bindIntern(p->ai_addr, p->ai_addrlen, prt, reuseaddr)) {
	   //printd(0, "qore_socket_private::bindINET(family: %d) bound: name: %s service: %s f: %d st: %d p: %d\n", family, name ? name : "(null)", service ? service : "(null)", p->ai_family, p->ai_socktype, p->ai_protocol);
	    return 0;
	 }

	 en = sock_get_raw_error();
	 //printd(0, "qore_socket_private::bindINET() failed to bind: name: %s service: %s f: %d st: %d p: %d, errno: %d (%s)\n", name ? name : "(null)", service ? service : "(null)", p->ai_family, p->ai_socktype, p->ai_protocol, en, strerror(en));
      }

      // if no bind was possible, then raise an exception
      qore_socket_error_intern(en, xsink, "SOCKET-BIND-ERROR", "error binding on socket", 0, name, service);
      return -1;
   }

   DLLLOCAL QoreHashNode *getPeerInfo(ExceptionSink *xsink) const {
      if (sock == QORE_INVALID_SOCKET) {
	 xsink->raiseException("SOCKET-GETPEERINFO-ERROR", "socket is not open()");
	 return 0;
      }

      struct sockaddr_storage addr;

      socklen_t len = sizeof addr;
      if (getpeername(sock, (struct sockaddr*)&addr, &len)) {
	 qore_socket_error(xsink, "SOCKET-GETPEERINFO-ERROR", "error in getpeername()");
	 return 0;
      }

      return getAddrInfo(addr, len);
   }

   DLLLOCAL QoreHashNode *getSocketInfo(ExceptionSink *xsink) const {
      if (sock == QORE_INVALID_SOCKET) {
	 xsink->raiseException("SOCKET-GETSOCKETINFO-ERROR", "socket is not open()");
	 return 0;
      }

      struct sockaddr_storage addr;
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
      // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
      int len = sizeof addr;
#else
      socklen_t len = sizeof addr;
#endif

      if (getsockname(sock, (struct sockaddr*)&addr, &len)) {
	 qore_socket_error(xsink, "SOCKET-GETSOCKETINFO-ERROR", "error in getsockname()");
	 return 0;
      }

      return getAddrInfo(addr, len);
   }

   DLLLOCAL QoreHashNode *getAddrInfo(const struct sockaddr_storage &addr, socklen_t len) const {
      QoreHashNode *h = new QoreHashNode;

      if (addr.ss_family == AF_INET || addr.ss_family == AF_INET6) {
	 char host[NI_MAXHOST + 1];

	 if (!getnameinfo((struct sockaddr *)&addr, get_in_len((struct sockaddr *)&addr), host, sizeof(host), 0, 0, 0)) {
	    QoreStringNode *hoststr = new QoreStringNode(host);
	    h->setKeyValue("hostname", hoststr, 0);
	    h->setKeyValue("hostname_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, hoststr->getBuffer()), 0);
	 }

	 // get ipv4 or ipv6 address
	 char ifname[INET6_ADDRSTRLEN];
	 if (inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), ifname, sizeof(ifname))) {
	    //printd(5, "inet_ntop() '%s' host: '%s'\n", ifname, host);
	    QoreStringNode *addrstr = new QoreStringNode(ifname);
	    h->setKeyValue("address", addrstr, 0);
	    h->setKeyValue("address_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, addrstr->getBuffer()), 0);
	 }

	 int tport;
	 if (addr.ss_family == AF_INET) {
	    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
	    tport = ntohs(s->sin_port);
	 }
	 else {
	    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
	    tport = ntohs(s->sin6_port);
	 }

	 h->setKeyValue("port", new QoreBigIntNode(tport), 0);
      }
#if (!defined _WIN32 && !defined __WIN32__) || defined __CYGWIN__
      else if (addr.ss_family == AF_UNIX) {
	 struct sockaddr_un *addr_un = (struct sockaddr_un *)&addr;
	 QoreStringNode *addrstr = new QoreStringNode(addr_un->sun_path);
	 h->setKeyValue("address", addrstr, 0); 
	 h->setKeyValue("address_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, addrstr->getBuffer()), 0);
      }
#endif

      h->setKeyValue("family", new QoreBigIntNode(addr.ss_family), 0);
      h->setKeyValue("familystr", new QoreStringNode(QoreAddrInfo::getFamilyName(addr.ss_family)), 0);

      return h;
   }

   // set backwards-compatible object members on accept
   // to be (hopefully) deleted in a future version of qore
   void setAccept(QoreObject *o) {
      struct sockaddr_storage addr;

      socklen_t len = sizeof addr;
      if (getpeername(sock, (struct sockaddr*)&addr, &len))
	 return;

      if (addr.ss_family == AF_INET || addr.ss_family == AF_INET6) {
	 // get ipv4 or ipv6 address
	 char ifname[INET6_ADDRSTRLEN];
	 if (inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), ifname, sizeof(ifname))) {
	    //printd(5, "inet_ntop() '%s' host: '%s'\n", ifname, host);
	    o->setValue("source", new QoreStringNode(ifname), 0);
	 }

	 char host[NI_MAXHOST + 1];
	 if (!getnameinfo((struct sockaddr *)&addr, get_in_len((struct sockaddr *)&addr), host, sizeof(host), 0, 0, 0))
	    o->setValue("source_host", new QoreStringNode(host), 0);
      }
#if (!defined _WIN32 && !defined __WIN32__) || defined __CYGWIN__
      else if (addr.ss_family == AF_UNIX) {
	 QoreStringNode *astr = new QoreStringNode(enc);
	 struct sockaddr_un *addr_un = (struct sockaddr_un *)&addr;
	 astr->sprintf("UNIX socket: %s", addr_un->sun_path);
	 o->setValue("source", astr, 0); 
	 o->setValue("source_host", new QoreStringNode("localhost"), 0);
      }
#endif
   }
   
   DLLLOCAL qore_offset_t recv(ExceptionSink* xsink, const char* meth, char *buf, qore_size_t bs, int flags, int timeout, bool do_event = true) {
      if (sock == QORE_INVALID_SOCKET) {
	 if (xsink)
	    se_not_open(meth, xsink);
	 return QSE_NOT_OPEN;
      }

      assert(meth);

      //printd(5, "qore_socket_private::recv(buf=%p, bs=%d, flags=%d, timeout=%d, do_event=%d) this=%p ssl=%d\n", buf, (int)bs, flags, timeout, (int)do_event, this, ssl);

      qore_offset_t rc;
      if (!ssl) {
	 if (timeout != -1 && !isDataAvailable(timeout, meth, xsink)) {
	    if (xsink) {
	       if (*xsink)
		  return -1;
	       se_timeout(meth, timeout, xsink);
	    }

	    return QSE_TIMEOUT;
	 }

	 while (true) {
#ifdef DEBUG
	    errno = 0;
#endif
	    rc = ::recv(sock, buf, bs, flags);
	    if (rc == QORE_SOCKET_ERROR) {
	       sock_get_error();
	       if (errno == EINTR)
		  continue;
#ifdef ECONNRESET
	       if (errno == ECONNRESET) {
		  if (xsink)
		     se_closed(meth, xsink);
		  close();
	       }
	       else
#endif
	       if (xsink)
		  qore_socket_error(xsink, "SOCKET-RECV-ERROR", "error in recv()", meth);
	       break;
	    }
	    //printd(5, "qore_socket_private::recv(%d, %p, %ld, %d) rc=%ld errno=%d\n", sock, buf, bs, flags, rc, errno);
	    // try again if we were interrupted by a signal
	    if (rc >= 0)
	       break;
	 }
      }
      else
	 rc = ssl->read(meth, buf, bs, timeout, xsink);

      if (rc > 0 && do_event) {
	 // register event
	 do_read_event(rc, rc);
      }

      return rc;
   }

   //! read until \\r\\n\\r\\n and return the string
   DLLLOCAL QoreStringNode* readHTTPData(ExceptionSink* xsink, const char* meth, int timeout, qore_offset_t& rc, int state = -1) {
      assert(meth);

      // state:
      //   0 = '\r' received
      //   1 = '\r\n' received
      //   2 = '\r\n\r' received
      //   3 = '\n' received
      // read in HHTP header until \r\n\r\n or \n\n from socket
      QoreStringNodeHolder hdr(new QoreStringNode(enc));

      qore_size_t count = 0;

      while (true) {
	 char c;

	 rc = recv(xsink, meth, &c, 1, 0, timeout, false);
	 //printd(5, "read char: %c (%03d) (old state: %d)\n", c > 30 ? c : '?', c, state);
	 if (rc <= 0) {
	    if (xsink && !*xsink) {
	       if (!count)
		  se_closed(meth, xsink);
	       else
		  xsink->raiseExceptionArg("SOCKET-HTTP-ERROR", hdr.release(), "socket closed on remote end while reading header data after reading "QSD" byte%s", count, count == 1 ? "" : "s");
	    }
	    //printd(5, "qore_socket_private::readHTTPData(timeout=%d) hdr='%s' (len: %d), rc="QSD", errno=%d: '%s'\n", timeout, hdr->getBuffer(), hdr->strlen(), rc, errno, strerror(errno));
	    return 0;
	 }
	 if (++count == QORE_MAX_HEADER_SIZE) {
	    if (xsink)
	       xsink->raiseException("SOCKET-HTTP-ERROR", "header size cannot exceed "QSD" bytes", count);
	    return 0;
	 }
	 
	 // check if we can progress to the next state
	 if (state == -1 && c == '\n') {
	    state = 3;
	    continue;
	 }
	 else if (state == -1 && c == '\r') {
	    state = 0;
	    continue;
	 }
	 else if (state > 0 && c == '\n')
	    break;
	 if (!state && c == '\n') {
	    state = 1;
	    continue;
	 }
	 else if (state == 1 && c == '\r') {
	    state = 2;
	    continue;
	 }
	 else {
	    if (!state)
	       hdr->concat('\r');
	    else if (state == 1)
	       hdr->concat("\r\n");
	    else if (state == 2)
	       hdr->concat("\r\n\r");
	    else if (state == 3)
	       hdr->concat('\n');
	    state = -1;
	    hdr->concat(c);
	 }
      }
      hdr->concat('\n');
      
      //printd(5, "qore_socket_private::readHTTPData(timeout=%d) hdr='%s' (%d)\n", timeout, hdr->getBuffer(), hdr->strlen());
      
      return hdr.release();
   }

   DLLLOCAL QoreStringNode* recv(qore_offset_t bufsize, int timeout, qore_offset_t& rc, ExceptionSink* xsink) {
      qore_size_t bs = bufsize > 0 && bufsize < DEFAULT_SOCKET_BUFSIZE ? bufsize : DEFAULT_SOCKET_BUFSIZE;

      QoreStringNode *str = new QoreStringNode(enc);

      char *buf = (char *)malloc(sizeof(char) * bs);
      ON_BLOCK_EXIT(free, buf);

      qore_size_t br = 0; // bytes received
      while (true) {
	 rc = recv(xsink, "recv", buf, bs, 0, timeout, false);

	 if (rc <= 0) {
	    printd(5, "qore_socket_private::recv(%d, %d) bs="QSD", br="QSD", rc="QSD", errno=%d (%s)\n", bufsize, timeout, bs, br, rc, errno, strerror(errno));

	    if (rc || !br || (!rc && bufsize > 0)) {
	       str->deref();
	       str = 0;
	    }
	    break;
	 }

	 str->concat(buf, rc);
	 br += rc;

	 // register event
	 do_read_event(rc, br, bufsize);

	 if (bufsize > 0) {
	    if (br >= (qore_size_t)bufsize)
	       break;
	    if (bufsize - br < bs)
	       bs = bufsize - br;
	 }
      }

      printd(5, "qore_socket_private::recv() received "QSD" byte(s), bufsize="QSD", strlen="QSD" str='%s'\n", br, bufsize, (size_t)(str ? str->strlen() : 0), str ? str->getBuffer() : "n/a");
      // "fix" return code value if no buffer size was set
      if (bufsize <= 0 && !rc)
	 rc = 1;
      return str;
   }

   DLLLOCAL QoreStringNode* recv(int timeout, qore_offset_t& rc, ExceptionSink* xsink) {
      // perform first read with timeout
      char *buf = (char *)malloc(sizeof(char) * (DEFAULT_SOCKET_BUFSIZE + 1));
      rc = recv(xsink, "recv", buf, DEFAULT_SOCKET_BUFSIZE, 0, timeout, false);
      if (rc <= 0) {
	 free(buf);
	 return 0;
      }
      qore_size_t rd = rc;

      // register event
      do_read_event(rc, rd);

      // keep reading data until no more data is available without a timeout
      if (isDataAvailable(0, "recv", xsink)) {
	 int tot = DEFAULT_SOCKET_BUFSIZE + 1;
	 do {
	    if ((tot - rd) < DEFAULT_SOCKET_BUFSIZE) {
	       tot += (DEFAULT_SOCKET_BUFSIZE + (tot >> 1));
	       buf = (char *)realloc(buf, tot);
	    }
	    rc = recv(xsink, "recv", buf + rd, tot - rd - 1, 0, 0, false);
	    //printd(0, "qore_socket_private::recv(to=%d) rc="QSD" rd="QSD"\n", timeout, rc, rd);
	    // if the remote end has closed the connection, return what we have
	    if (!rc)
	       break;
	    if (rc < 0) {
	       free(buf);
	       return 0;
	    }
	    rd += rc;

	    // register event
	    do_read_event(rc, rd);
	 } while (isDataAvailable(0, "recv", xsink));
      }

      if (*xsink) {
	 free(buf);
	 return 0;
      }

      buf[rd] = '\0';
      rc = rd;
      return new QoreStringNode(buf, rd, rd + 1, enc);
   }

   DLLLOCAL BinaryNode *recvBinary(qore_offset_t bufsize, int timeout, qore_offset_t& rc, ExceptionSink* xsink) {
      qore_size_t bs = bufsize > 0 && bufsize < DEFAULT_SOCKET_BUFSIZE ? bufsize : DEFAULT_SOCKET_BUFSIZE;

      SimpleRefHolder<BinaryNode> b(new BinaryNode);

      char *buf = (char *)malloc(sizeof(char) * bs);
      qore_size_t br = 0; // bytes received
      while (true) {
	 rc = recv(xsink, "recvBinary", buf, bs, 0, timeout);
	 if (rc <= 0) {
	    if (rc || !br || (!rc && bufsize > 0))
	       b = 0; // free binary object

	    break;
	 }
	 b->append(buf, rc);
	 br += rc;

	 if (bufsize > 0) {
	    if (bufsize - br < bs)
	       bs = bufsize - br;
	    if (br >= (qore_size_t)bufsize)
	       break;
	 }
      }
      free(buf);
      // "fix" return code value if no buffer size was set
      if (bufsize <= 0 && !rc)
	 rc = 1;
      printd(5, "qore_socket_private::recvBinary() received "QSD" byte(s), bufsize="QSD", strlen="QSD"\n", br, bufsize, b->size());
      return b.release();
   }

   DLLLOCAL BinaryNode *recvBinary(int timeout, qore_offset_t& rc, ExceptionSink* xsink) {
      //printd(5, "QoreSocket::recvBinary(%d, "QSD") this=%p\n", timeout, rc, this);
      // perform first read with timeout
      char *buf = (char *)malloc(sizeof(char) * DEFAULT_SOCKET_BUFSIZE);
      rc = recv(xsink, "recvBinary", buf, DEFAULT_SOCKET_BUFSIZE, 0, timeout, false);
      if (rc <= 0) {
	 free(buf);
	 return 0;
      }
      qore_size_t rd = rc;

      // register event
      do_read_event(rc, rd);

      // keep reading data until no more data is available without a timeout
      if (isDataAvailable(0, "recvBinary", xsink)) {
	 int tot = DEFAULT_SOCKET_BUFSIZE;
	 do {
	    if ((tot - rd) < DEFAULT_SOCKET_BUFSIZE) {
	       tot += (DEFAULT_SOCKET_BUFSIZE + (tot >> 1));
	       buf = (char *)realloc(buf, tot);
	    }
	    rc = recv(xsink, "recvBinary", buf + rd, tot - rd, 0, 0, false);
	    // if the remote end has closed the connection, return what we have
	    if (!rc)
	       break;
	    if (rc < 0) {
	       free(buf);
	       return 0;
	    }
	    rd += rc;

	    // register event
	    do_read_event(rc, rd);
	 } while (isDataAvailable(0, "recvBinary", xsink));
      }

      if (*xsink) {
	 free(buf);
	 return 0;
      }

      rc = rd;
      return new BinaryNode(buf, rd);
   }

   DLLLOCAL AbstractQoreNode* readHTTPHeader(ExceptionSink* xsink, QoreHashNode *info, int timeout, qore_offset_t& rc, int source) {
      QoreStringNodeHolder hdr(readHTTPData(xsink, "readHTTPHeader", timeout, rc));
      if (!hdr) {
	 assert(*xsink);
	 return 0;
      }
      assert(rc > 0);

      const char *buf = hdr->getBuffer();
      //printd(5, "HTTP header=%s", buf);

      char *p;
      if ((p = (char *)strstr(buf, "\r\n"))) {
	 *p = '\0';
	 p += 2;
      }
      else if ((p = (char *)strchr(buf, '\n'))) {
	 *p = '\0';
	 ++p;
      }
      // readHTTPData will only return a string that satisifies one of the above conditions
      else 
	 assert(false);

      char *t1;
      if (!(t1 = (char *)strstr(buf, "HTTP/"))) {
	 if (xsink) {
	    xsink->raiseException("SOCKET-HTTP-ERROR", "missing HTTP version string in first header line in Socket::readHTTPHeader()");
	    return 0;
	 }
	 return hdr.release();
      }

      QoreHashNode *h = new QoreHashNode;

#if 0
      h->setKeyValue("dbg_hdr", new QoreStringNode(buf), 0);
#endif

      // get version
      h->setKeyValue("http_version", new QoreStringNode(t1 + 5, 3, enc), 0);

      // if we are getting a response
      if (t1 == buf) {
	 char *t2 = (char *)strchr(buf + 8, ' ');
	 if (t2) {
	    t2++;
	    if (isdigit(*(t2))) {
	       h->setKeyValue("status_code", new QoreBigIntNode(atoi(t2)), 0);
	       if (strlen(t2) > 4) {
		  h->setKeyValue("status_message", new QoreStringNode(t2 + 4), 0);
	       }
	    }
	 }
	 if (info)
	    info->setKeyValue("response-uri", new QoreStringNode(buf), 0);
      }
      else { // get method and path
	 char *t2 = (char *)strchr(buf, ' ');
	 if (t2) {
	    *t2 = '\0';
	    h->setKeyValue("method", new QoreStringNode(buf), 0);
	    t2++;
	    t1 = strchr(t2, ' ');
	    if (t1) {
	       *t1 = '\0';
	       //printd(5, "found path '%s'\n", t2);
	       // the path is returned as-is with no decodings - use decode_url() to decode
	       h->setKeyValue("path", new QoreStringNode(t2, enc), 0);
	    }
	    if (info)
	       info->setKeyValue("request-uri", new QoreStringNode(buf), 0);
	 }
      }
   
      convertHeaderToHash(h, p);
      do_read_http_header(QORE_EVENT_HTTP_MESSAGE_RECEIVED, h, source);

      return h;
   }

   DLLLOCAL int send(ExceptionSink* xsink, const char* mname, const char *buf, qore_size_t size, int timeout_ms = -1) {
      if (sock == QORE_INVALID_SOCKET) {
	 if (xsink)
	    se_not_open(mname, xsink);

	 return QSE_NOT_OPEN;
      }

      // set the non-blocking flag (for use with non-ssl connections)
      bool nb = (timeout_ms >= 0);
      // set non-blocking I/O (and restore on exit) if we have a timeout and a non-ssl connection
      OptionalNonBlockingHelper onbh(*this, !ssl && nb, xsink);
      if (*xsink)
         return -1;

      qore_offset_t rc;
      qore_size_t bs = 0;
      while (true) {
         if (ssl) {
            // SSL_MODE_ENABLE_PARTIAL_WRITE is enabled so we can get finer-grained socket events for do_send_event() below
            rc = ssl->write(mname, buf + bs, size - bs, timeout_ms, xsink);
         }
         else
            while (true) {
               rc = ::send(sock, buf + bs, size - bs, 0);
               //printd(5, "qore_socket_private::send() this: %p Socket::%s() buf: %p size: "QLLD" timeout_ms: %d ssl: %p nb: %d bs: "QLLD" rc: "QLLD"\n", this, mname, buf, size, timeout_ms, ssl, nb, bs, rc);
               // try again if we were interrupted by a signal
               if (rc >= 0)
                  break;
	       sock_get_error();
               // check that the send finishes before the timeout if we are using non-blocking I/O
               if (nb && (errno == EAGAIN
#ifdef EWOULDBLOCK
			  || errno == EWOULDBLOCK
#endif
		      )) {
                  if (!isWriteFinished(timeout_ms, mname, xsink)) {
                     if (xsink) {
			if (*xsink)
			   return -1;
                        se_timeout(mname, timeout_ms, xsink);
		     }
                     rc = QSE_TIMEOUT;
                     break;
                  }
                  continue;
               }
               if (errno != EINTR) {
                  if (xsink)
                     xsink->raiseErrnoException("SOCKET-SEND-ERROR", errno, "error while executing Socket::%s()", mname);

#ifdef EPIPE
		  if (errno == EPIPE)
		     close();
#endif
#ifdef ECONNRESET
		  if (errno == ECONNRESET)
		     close();
#endif

                  break;
               }
            }

	 //printd(5, "qore_socket_private::send() bs=%ld rc="QSD" len="QSD" (total="QSD")\n", bs, rc, size - bs, size);
	 if (rc < 0)
	    return rc;

	 bs += rc;

	 do_send_event(rc, bs, size);

	 if (bs >= size)
	    break;
      }

      //printd(5, "qore_socket_private::send() sent "QSD" bytes (size="QSD")\n", bs, size);
      return 0;
   }

   DLLLOCAL int sendHTTPMessage(ExceptionSink* xsink, QoreHashNode *info, const char *method, const char *path, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source, int timeout_ms = -1) {
      // prepare header string
      QoreString hdr(enc);

      hdr.sprintf("%s %s HTTP/%s", method, path && path[0] ? path : "/", http_version);

      // write request-uri key if info hash is non-null
      if (info)
	 info->setKeyValue("request-uri", new QoreStringNode(hdr), 0);

      do_send_http_message(hdr, headers, source);
      hdr.concat("\r\n");
      
      // insert headers
      do_headers(hdr, headers, size && data ? size : 0);

      //printd(5, "qore_socket_private::sendHTTPMessage() hdr: %s\n", hdr.getBuffer());

      int rc;
      if ((rc = send(xsink, "sendHTTPMessage", hdr.getBuffer(), hdr.strlen(), timeout_ms)))
	 return rc;
      
      return size && data ? send(xsink, "sendHTTPMessage", (char *)data, size, timeout_ms) : 0;
   }

   DLLLOCAL int sendHTTPResponse(ExceptionSink* xsink, int code, const char *desc, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source, int timeout_ms = -1) {
      // prepare header string
      QoreString hdr(enc);

      hdr.sprintf("HTTP/%s %03d %s", http_version, code, desc);

      do_send_http_message(hdr, headers, source);

      hdr.concat("\r\n");

      do_headers(hdr, headers, size && data ? size : 0, true);
   
      //printd(5, "QoreSocket::sendHTTPResponse() data: %p size: %ld hdr: %s", data, size, hdr.getBuffer());
   
      int rc;
      if ((rc = send(xsink, "sendHTTPResponse", hdr.getBuffer(), hdr.strlen(), timeout_ms)))
	 return rc;

      if (size && data)
	 return send(xsink, "sendHTTPResponse", (char *)data, size, timeout_ms);
      
      return 0;
   }

   // static method
   static void convertHeaderToHash(QoreHashNode *h, char *p) {
      while (*p) {
	 char *buf = p;
      
	 if ((p = strstr(buf, "\r\n"))) {
	    *p = '\0';
	    p += 2;
	 }
	 else if ((p = strchr(buf, '\n'))) {
	    *p = '\0';
	    p++;
	 }
	 else
	    break;
	 char *t = strchr(buf, ':');
	 if (!t)
	    break;
	 *t = '\0';
	 t++;
	 while (t && isblank(*t))
	    t++;
	 strtolower(buf);
	 //printd(5, "setting %s = '%s'\n", buf, t);
	 
	 AbstractQoreNode *val = new QoreStringNode(t);

	 // see if header exists, and if so make it a list and add value to the list
	 hash_assignment_priv ha(*h, buf);
	 if (*ha) {
	    QoreListNode *l;
	    if ((*ha)->getType() == NT_LIST)
	       l = reinterpret_cast<QoreListNode *>(*ha);
	    else {
	       l = new QoreListNode;
	       l->push(ha.swap(l));
	    }
	    l->push(val);
	 }
	 else // otherwise set header normally
	    ha.assign(val, 0);
      }
   }
};

int SSLSocketHelper::doSSLRW(const char* mname, void* buf, int size, int timeout_ms, bool read, ExceptionSink* xsink) {
   if (timeout_ms < 0) {
      while (true) {
         int rc = read ? SSL_read(ssl, buf, size) : SSL_write(ssl, buf, size);
         if (rc < 0) {
	    // we set SSL_MODE_AUTO_RETRY so there should never be any need to retry
#ifdef DEBUG
            int err = SSL_get_error(ssl, rc);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
	       assert(false);
               continue;
	    }
#endif

            if (xsink && !sslError(xsink, mname, read ? "SSL_read" : "SSL_write", false))
               rc = 0;
         }
         return rc;
      }
   }

   // set non blocking
   OptionalNonBlockingHelper(qs, true, xsink);
   if (*xsink)
      return -1;

   int rc;
   while (true) {
      rc = read ? SSL_read(ssl, buf, size) : SSL_write(ssl, buf, size);

      if (rc >= 0)
         break;

      if (rc < 0) {
         int err = SSL_get_error(ssl, rc);

         if (err == SSL_ERROR_WANT_READ) {
            if (!qs.isDataAvailable(timeout_ms, mname, xsink)) {
               if (xsink) {
		  if (*xsink)
		     return -1;
                  se_timeout(mname, timeout_ms, xsink);
	       }
               rc = QSE_TIMEOUT;
               break;
            }
         }
         else if (err == SSL_ERROR_WANT_WRITE) {
            if (!qs.isWriteFinished(timeout_ms, mname, xsink)) {
               if (xsink) {
		  if (*xsink)
		     return -1;
                  se_timeout(mname, timeout_ms, xsink);
	       }
               rc = QSE_TIMEOUT;
               break;
            }
         }
         // here we allow the remote side to disconnect and return 0 the first time just like regular recv()
         else if (read && err == SSL_ERROR_ZERO_RETURN) {
            rc = 0;
            break;
         }
         else if (err == SSL_ERROR_SYSCALL) {
            if (xsink) {
               if (!sslError(xsink, mname, read ? "SSL_read" : "SSL_write", false)) {
                  if (!rc)
                     xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported an EOF condition that violates the SSL protocol while calling SSL_%s()", mname, read ? "read" : "write");
                  else if (rc == -1) {
                     xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the openssl library reported an I/O error while calling SSL_%s()", mname, read ? "read" : "write");

#ifdef ECONNRESET
                     // close the socket if connection reset received
		     if (sock_get_error() == ECONNRESET)
			qs.close();
#endif
		  }
                  else
                     xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported error code %d in SSL_%s() but the error queue is empty", mname, rc, read ? "read" : "write");
               }
            }

            rc = QSE_SSL_ERR;
            break;
         }
         else {
            //printd(0, "SSLSocketHelper::doSSLRW(buf=%p, size=%d, to=%d) rc=%d err=%d\n", buf, size, timeout_ms, rc, err);
            if (xsink && !sslError(xsink, mname, read ? "SSL_read" : "SSL_write", false))
               rc = 0;
            else
               rc = QSE_SSL_ERR;
            break;
         }
      }
   }

   //printd(5, "SSLSocketHelper::doSSLRW(buf: %p, size: %d, to: %d, read: %d) rc: %d\n", buf, size, timeout_ms, (int)read, rc);
   return rc;
}

DLLLOCAL OptionalNonBlockingHelper::OptionalNonBlockingHelper(qore_socket_private& s, bool n_set, ExceptionSink* xs) : sock(s), xsink(xs), set(n_set) {
   if (set) {
      //printd(5, "OptionalNonBlockingHelper::OptionalNonBlockingHelper() this: %p\n", this);
      sock.set_non_blocking(true, xsink);
   }
}

DLLLOCAL OptionalNonBlockingHelper::~OptionalNonBlockingHelper() {
   if (set) {
      //printd(5, "OptionalNonBlockingHelper::~OptionalNonBlockingHelper() this: %p\n", this);
      sock.set_non_blocking(false, xsink);
   }
}

int SSLSocketHelper::read(const char* mname, char *buf, int size, int timeout_ms, ExceptionSink* xsink) {
   return doSSLRW(mname, buf, size, timeout_ms, true, xsink);
}

// returns true if an error was raised, false if not
bool SSLSocketHelper::sslError(ExceptionSink* xsink, const char* mname, const char* func, bool always_error) {
   long e = ERR_get_error();
   bool closed = false;
   do {
      if (!e || e == SSL_ERROR_ZERO_RETURN) {
	 closed = true;
	 if (always_error)
	    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the %s() call could not be completed because the TLS/SSL connection was terminated", mname, func);
      }
      else {
	 char buf[121];
	 ERR_error_string(e, buf);
	 xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): %s(): %s", mname, func, buf);
#ifdef ECONNRESET
	 // close the socket if connection reset received
	 if (e == SSL_ERROR_SYSCALL && sock_get_error() == ECONNRESET)
	    qs.close();
#endif
      }
   } while ((e = ERR_get_error()));
   
   return *xsink || closed;
}

QoreSocket::QoreSocket() : priv(new qore_socket_private) {
}

QoreSocket::QoreSocket(int n_sock, int n_sfamily, int n_stype, int n_prot, const QoreEncoding *n_enc) : priv(new qore_socket_private(n_sock, n_sfamily, n_stype, n_prot, n_enc)) {
}

QoreSocket::~QoreSocket() {
   delete priv;
}

int QoreSocket::setNoDelay(int nodelay) {
   return setsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (SETSOCKOPT_ARG_4)&nodelay, sizeof(int));
}

int QoreSocket::getNoDelay() const {
   int rc;
   socklen_t optlen = sizeof(int);
   int sorc = getsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (GETSOCKOPT_ARG_4)&rc, &optlen);
   //printd(5, "Socket::getNoDelay() sorc=%d rc=%d optlen=%d\n", sorc, rc, optlen);
   if (sorc)
       return sorc;
   return rc;
}

int QoreSocket::close() {
   return priv->close();
}

int QoreSocket::shutdown() {
   int rc;
   if (priv->sock != QORE_INVALID_SOCKET)
      rc = ::shutdown(priv->sock, SHUTDOWN_ARG); 
   else 
      rc = 0; 
   
   return rc;
}

int QoreSocket::shutdownSSL(ExceptionSink *xsink) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return 0;
   if (!priv->ssl)
      return 0;
   return priv->ssl->shutdown(xsink);
}

int QoreSocket::getSocket() const {
   return priv->sock; 
}

const QoreEncoding *QoreSocket::getEncoding() const {
   return priv->enc; 
}

void QoreSocket::setEncoding(const QoreEncoding *id) { 
   priv->enc = id; 
} 

bool QoreSocket::isOpen() const { 
   return (bool)(priv->sock != QORE_INVALID_SOCKET); 
}

const char *QoreSocket::getSSLCipherName() const {
   if (!priv->ssl)
      return 0;
   return priv->ssl->getCipherName();
}

const char *QoreSocket::getSSLCipherVersion() const {
   if (!priv->ssl)
      return 0;
   return priv->ssl->getCipherVersion();
}

bool QoreSocket::isSecure() const {
   return (bool)priv->ssl;
}

long QoreSocket::verifyPeerCertificate() const {
   if (!priv->ssl)
      return -1;
   return priv->ssl->verifyPeerCertificate();
}

// hardcoded to SOCK_STREAM (tcp only)
int QoreSocket::connectINET(const char *host, int prt, int timeout_ms, ExceptionSink *xsink) {
   QoreString service;
   service.sprintf("%d", prt);

   return priv->connectINET(host, service.getBuffer(), timeout_ms, xsink);
}

int QoreSocket::connectINET(const char *host, int prt, ExceptionSink *xsink) {
   QoreString service;
   service.sprintf("%d", prt);

   return priv->connectINET(host, service.getBuffer(), -1, xsink);
}

int QoreSocket::connectINET2(const char *name, const char *service, int family, int socktype, int protocol, int timeout_ms, ExceptionSink *xsink) {
   return priv->connectINET(name, service, timeout_ms, xsink, family, socktype, protocol);
}

int QoreSocket::connectUNIX(const char *p, ExceptionSink *xsink) {
   return priv->connectUNIX(p, SOCK_STREAM, 0, xsink);
}

int QoreSocket::connectUNIX(const char *p, int sock_type, int protocol, ExceptionSink *xsink) {
   return priv->connectUNIX(p, sock_type, protocol, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket
// for AF_INET sockets:
// * QoreSocket::connect("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connect("filename");
int QoreSocket::connect(const char *name, int timeout_ms, ExceptionSink *xsink) {
   const char *p;
   int rc;

   if ((p = strrchr(name, ':'))) {
      QoreString host(name, p - name);
      QoreString service(p + 1);
      // if the address is an ipv6 address like: [<addr>], then connect as ipv6
      if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
	 host.terminate(host.strlen() - 1);
	 //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.getBuffer() + 1, service.getBuffer());
	 rc = priv->connectINET(host.getBuffer() + 1, service.getBuffer(), timeout_ms, xsink, AF_INET6);
      }
      else 
	 rc = priv->connectINET(host.getBuffer(), service.getBuffer(), timeout_ms, xsink);
   }
   else {
      // else assume it's a file name for a UNIX domain socket
      rc = priv->connectUNIX(name, SOCK_STREAM, 0, xsink);
   }

   return rc;
}

int QoreSocket::connect(const char *name, ExceptionSink *xsink) {
   return connect(name, -1, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket and negotiates an SSL connection
// for AF_INET sockets:
// * QoreSocket::connectSSL("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connectSSL("filename");
int QoreSocket::connectSSL(const char *name, int timeout_ms, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   const char *p;
   int rc;

   if ((p = strchr(name, ':'))) {
      QoreString host(name, p - name);
      QoreString service(p + 1);
      // if the address is an ipv6 address like: [<addr>], then connect as ipv6
      if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
	 host.terminate(host.strlen() - 1);
	 //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.getBuffer() + 1, service.getBuffer());
	 rc = connectINET2SSL(host.getBuffer() + 1, service.getBuffer(), AF_INET6, SOCK_STREAM, 0, timeout_ms, cert, pkey, xsink);
      }
      else 
	 rc = connectINET2SSL(host.getBuffer(), service.getBuffer(), AF_UNSPEC, SOCK_STREAM, 0, timeout_ms, cert, pkey, xsink);
   }
   else {
      // else assume it's a file name for a UNIX domain socket
      rc = connectUNIXSSL(name, SOCK_STREAM, 0, cert, pkey, xsink);
   }

   return rc;
}

int QoreSocket::connectSSL(const char *name, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   return connectSSL(name, -1, cert, pkey, xsink);
}

int QoreSocket::connectINETSSL(const char *host, int prt, int timeout_ms, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   QoreString service;
   service.sprintf("%d", prt);

   int rc = priv->connectINET(host, service.getBuffer(), timeout_ms, xsink);
   if (rc)
      return rc;
   return priv->upgradeClientToSSLIntern("connectINETSSL", cert, pkey, xsink);
}

int QoreSocket::connectINETSSL(const char *host, int prt, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   return connectINETSSL(host, prt, -1, cert, pkey, xsink);
}

int QoreSocket::connectINET2SSL(const char *name, const char *service, int family, int sock_type, int protocol, int timeout_ms, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   int rc = connectINET2(name, service, family, sock_type, protocol, timeout_ms, xsink);
   if (rc)
      return rc;
   return priv->upgradeClientToSSLIntern("connectINET2SSL", cert, pkey, xsink);
}

int QoreSocket::connectUNIXSSL(const char *p, int sock_type, int protocol, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   int rc = connectUNIX(p, sock_type, protocol, xsink);
   if (rc)
      return rc;
   return priv->upgradeClientToSSLIntern("connectUNIXSSL", cert, pkey, xsink);
}

int QoreSocket::sendi1(char i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   int rc = priv->send(0, "sendi1", &i, 1);

   if (rc < 0)
      return -1;

   return 0;
}

int QoreSocket::sendi2(short i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to network byte order
   i = htons(i);
   return priv->send(0, "sendi2", (char *)&i, 2);
}

int QoreSocket::sendi4(int i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to network byte order
   i = htonl(i);
   return priv->send(0, "sendi4", (char *)&i, 4);
}

int QoreSocket::sendi8(int64 i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to network byte order
   i = i8MSB(i);
   return priv->send(0, "sendi8", (char *)&i, 8);
}

int QoreSocket::sendi2LSB(short i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to LSB byte order
   i = i2LSB(i);
   return priv->send(0, "sendi2LSB", (char *)&i, 2);
}

int QoreSocket::sendi4LSB(int i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to LSB byte order
   i = i4LSB(i);
   return priv->send(0, "sendi4LSB", (char *)&i, 4);
}

int QoreSocket::sendi8LSB(int64 i) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   // convert to LSB byte order
   i = i8LSB(i);
   return priv->send(0, "sendi8LSB", (char *)&i, 8);
}

int QoreSocket::sendi1(char i, int timeout_ms, ExceptionSink* xsink) {
   return priv->send(xsink, "sendi1", &i, 1, timeout_ms);
}

int QoreSocket::sendi2(short i, int timeout_ms, ExceptionSink* xsink) {
   // convert to network byte order
   i = htons(i);
   return priv->send(xsink, "sendi2", (char *)&i, 2, timeout_ms);
}

int QoreSocket::sendi4(int i, int timeout_ms, ExceptionSink* xsink) {
   // convert to network byte order
   i = htonl(i);
   return priv->send(xsink, "sendi4", (char *)&i, 4, timeout_ms);
}

int QoreSocket::sendi8(int64 i, int timeout_ms, ExceptionSink* xsink) {
   // convert to network byte order
   i = i8MSB(i);
   return priv->send(xsink, "sendi8", (char *)&i, 8, timeout_ms);
}

int QoreSocket::sendi2LSB(short i, int timeout_ms, ExceptionSink* xsink) {
   // convert to LSB byte order
   i = i2LSB(i);
   return priv->send(xsink, "sendi2LSB", (char *)&i, 2, timeout_ms);
}

int QoreSocket::sendi4LSB(int i, int timeout_ms, ExceptionSink* xsink) {
   // convert to LSB byte order
   i = i4LSB(i);
   return priv->send(xsink, "sendi4LSB", (char *)&i, 4, timeout_ms);
}

int QoreSocket::sendi8LSB(int64 i, int timeout_ms, ExceptionSink* xsink) {
   // convert to LSB byte order
   i = i8LSB(i);
   return priv->send(xsink, "sendi8LSB", (char *)&i, 8, timeout_ms);
}

// receive integer values and convert from network byte order
int QoreSocket::recvi1(int timeout, char *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   return (int)priv->recv(0, "recvi1", val, 1, 0, timeout);
}

int QoreSocket::recvi2(int timeout, short *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi2", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 2)
	 break;
   }

   *val = ntohs(*val);
   return 2;
}

int QoreSocket::recvi4(int timeout, int *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi4", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 4)
	 break;
   }

   *val = ntohl(*val);
   return 4;
}

int QoreSocket::recvi8(int timeout, int64 *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi8", buf + br, 8 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 8)
	 break;
   }

   *val = MSBi8(*val);
   return 8;
}

int QoreSocket::recvi2LSB(int timeout, short *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi2LSB", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 2)
	 break;
   }

   *val = LSBi2(*val);
   return 2;
}

int QoreSocket::recvi4LSB(int timeout, int *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi4LSB", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 4)
	 break;
   }

   *val = LSBi4(*val);
   return 4;
}

int QoreSocket::recvi8LSB(int timeout, int64 *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;

   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvi8LSB", buf + br, 8 - br, 0, timeout);
      if (rc <= 0)
	 return rc;

      br += rc;

      if (br >= 8)
	 break;
   }

   *val = LSBi8(*val);
   return 4;
}

int QoreSocket::recvu1(int timeout, unsigned char *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   
   return priv->recv(0, "recvu1", (char *)val, 1, 0, timeout);
}

int QoreSocket::recvu2(int timeout, unsigned short *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   
   char *buf = (char *)val;
   
   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvu2", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
	 return rc;
      
      br += rc;
      
      if (br >= 2)
	 break;
   }
   
   *val = ntohs(*val);
   return 2;
}

int QoreSocket::recvu4(int timeout, unsigned int *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   
   char *buf = (char *)val;
   
   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvu4", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
	 return rc;
      
      br += rc;
      
      if (br >= 4)
	 break;
   }
   
   *val = ntohl(*val);
   return 4;
}

int QoreSocket::recvu2LSB(int timeout, unsigned short *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   
   char *buf = (char *)val;
   
   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvu2LSB", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
	 return rc;
      
      br += rc;
      
      if (br >= 2)
	 break;
   }
   
   *val = LSBi2(*val);
   return 2;
}

int QoreSocket::recvu4LSB(int timeout, unsigned int *val) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   
   char *buf = (char *)val;
   
   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(0, "recvu4LSB", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
	 return rc;
      
      br += rc;
      
      if (br >= 4)
	 break;
   }
   
   *val = LSBi4(*val);
   return 4;
}

static int do_read_error(qore_offset_t rc, const char *method_name, int timeout_ms, ExceptionSink *xsink) {
   if (rc > 0)
      return 0;
   if (!*xsink)
      QoreSocket::doException(rc, method_name, timeout_ms, xsink);
   return -1;
}

int64 QoreSocket::recvi1(int timeout, char *val, ExceptionSink* xsink) {
   qore_offset_t rc = priv->recv(xsink, "recvi1", val, 1, 0, timeout);
   return rc <= 0 ? do_read_error(rc, "recvi1", timeout, xsink) : rc;
}

int64 QoreSocket::recvi2(int timeout, short *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi2", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi2", timeout, xsink);

      br += rc;

      if (br >= 2)
         break;
   }

   *val = ntohs(*val);
   return 2;
}

int64 QoreSocket::recvi4(int timeout, int *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi4", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi4", timeout, xsink);

      br += rc;

      if (br >= 4)
         break;
   }

   *val = ntohl(*val);
   return 4;
}

int64 QoreSocket::recvi8(int timeout, int64 *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi8", buf + br, 8 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi8", timeout, xsink);

      br += rc;

      if (br >= 8)
         break;
   }

   *val = MSBi8(*val);
   return 8;
}

int64 QoreSocket::recvi2LSB(int timeout, short *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi2LSB", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi2LSB", timeout, xsink);

      br += rc;

      if (br >= 2)
         break;
   }

   *val = LSBi2(*val);
   return 2;
}

int64 QoreSocket::recvi4LSB(int timeout, int *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi4LSB", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi4LSB", timeout, xsink);

      br += rc;

      if (br >= 4)
         break;
   }

   *val = LSBi4(*val);
   return 4;
}

int64 QoreSocket::recvi8LSB(int timeout, int64 *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvi8LSB", buf + br, 8 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvi8LSB", timeout, xsink);

      br += rc;

      if (br >= 8)
         break;
   }

   *val = LSBi8(*val);
   return 4;
}

int64 QoreSocket::recvu1(int timeout, unsigned char *val, ExceptionSink* xsink) {
   qore_offset_t rc = priv->recv(xsink, "recvu1", (char *)val, 1, 0, timeout);
   //printd(5, "QoreSocket::recvu1() val: %d rc: %lld *xsink: %d\n", (int)*val, rc, (int)(bool)*xsink);
   return rc <= 0 ? do_read_error(rc, "recvu1", timeout, xsink) : rc;
}

int64 QoreSocket::recvu2(int timeout, unsigned short *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvu2", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvu2", timeout, xsink);

      br += rc;

      if (br >= 2)
         break;
   }

   *val = ntohs(*val);
   return 2;
}

int64 QoreSocket::recvu4(int timeout, unsigned int *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvu4", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvu4", timeout, xsink);

      br += rc;

      if (br >= 4)
         break;
   }

   *val = ntohl(*val);
   return 4;
}

int64 QoreSocket::recvu2LSB(int timeout, unsigned short *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvu2LSB", buf + br, 2 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvu2LSB", timeout, xsink);;

      br += rc;

      if (br >= 2)
         break;
   }

   *val = LSBi2(*val);
   return 2;
}

int64 QoreSocket::recvu4LSB(int timeout, unsigned int *val, ExceptionSink* xsink) {
   char *buf = (char *)val;

   qore_offset_t br = 0;
   while (true) {
      qore_offset_t rc = priv->recv(xsink, "recvu4LSB", buf + br, 4 - br, 0, timeout);
      if (rc <= 0)
         return do_read_error(rc, "recvu4LSB", timeout, xsink);

      br += rc;

      if (br >= 4)
         break;
   }

   *val = LSBi4(*val);
   return 4;
}

int QoreSocket::send(int fd, qore_offset_t size) {
   if (priv->sock == QORE_INVALID_SOCKET || !size) {
      printd(5, "QoreSocket::send() ERROR: sock=%d size="QSD"\n", priv->sock, size);
      return -1;
   }

   char *buf = (char *)malloc(sizeof(char) * DEFAULT_SOCKET_BUFSIZE);

   qore_offset_t rc = 0;
   qore_size_t bs = 0;
   while (true) {
      // calculate bytes needed
      qore_size_t bn;
      if (size < 0)
	 bn = DEFAULT_SOCKET_BUFSIZE;
      else {
	 bn = size - bs;
	 if (bn > DEFAULT_SOCKET_BUFSIZE)
	    bn = DEFAULT_SOCKET_BUFSIZE;
      }
      rc = read(fd, buf, bn);
      if (!rc)
	 break;
      if (rc < 0) {
	 printd(5, "QoreSocket::send() read error: %s\n", strerror(errno));
	 break;
      }

      // send buffer
      int src = priv->send(0, "send", buf, rc);
      if (src < 0) {
	 printd(5, "QoreSocket::send() send error: %s\n", strerror(errno));
	 break;
      }
      bs += rc;
      if (size > 0 && bs >= (qore_size_t)size) {
	 rc = 0;
	 break;
      }
   }
   free(buf);
   return rc;
}

BinaryNode *QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, int *rc) {
   assert(rc);
   qore_offset_t nrc;
   BinaryNode* b = priv->recvBinary(bufsize, timeout, nrc, 0);
   *rc = (int)nrc;
   return b;
}

BinaryNode *QoreSocket::recvBinary(int timeout, int *rc) {
   assert(rc);
   qore_offset_t nrc;
   BinaryNode* b = priv->recvBinary(timeout, nrc, 0);
   *rc = (int)nrc;
   return b;
}

BinaryNode *QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
   assert(xsink);
   qore_offset_t rc;
   BinaryNodeHolder b(priv->recvBinary(bufsize, timeout, rc, xsink));
   return *xsink ? 0 : b.release();
}

BinaryNode *QoreSocket::recvBinary(int timeout, ExceptionSink* xsink) {
   assert(xsink);
   qore_offset_t rc;
   BinaryNodeHolder b(priv->recvBinary(timeout, rc, xsink));
   return *xsink ? 0 : b.release();
}

QoreStringNode *QoreSocket::recv(qore_offset_t bufsize, int timeout, int *rc) {
   assert(rc);
   qore_offset_t nrc;
   QoreStringNode* str = priv->recv(bufsize, timeout, nrc, 0);
   *rc = (int)nrc;
   return str;
}

QoreStringNode *QoreSocket::recv(int timeout, int *rc) {
   assert(rc);
   qore_offset_t nrc;
   QoreStringNode* str = priv->recv(timeout, nrc, 0);
   *rc = (int)nrc;
   return str;
}

QoreStringNode *QoreSocket::recv(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
   assert(xsink);
   qore_offset_t rc;
   QoreStringNodeHolder str(priv->recv(bufsize, timeout, rc, xsink));
   return *xsink ? 0 : str.release();
}

QoreStringNode *QoreSocket::recv(int timeout, ExceptionSink* xsink) {
   assert(xsink);
   qore_offset_t rc;
   QoreStringNodeHolder str(priv->recv(timeout, rc, xsink));
   return *xsink ? 0 : str.release();
}

// receive data and write to file descriptor
int QoreSocket::recv(int fd, qore_offset_t size, int timeout) {
   if (priv->sock == QORE_INVALID_SOCKET || !size)
      return -1;

   char *buf = (char *)malloc(sizeof(char) * DEFAULT_SOCKET_BUFSIZE);
   qore_offset_t br = 0;
   qore_offset_t rc;
   while (true) {
      // calculate bytes needed
      int bn;
      if (size == -1)
	 bn = DEFAULT_SOCKET_BUFSIZE;
      else {
	 bn = size - br;
	 if (bn > DEFAULT_SOCKET_BUFSIZE)
	    bn = DEFAULT_SOCKET_BUFSIZE;
      }

      rc = priv->recv(0, "recv", buf, bn, 0, timeout);
      if (rc <= 0)
	 break;
      br += rc;

      // write buffer to file descriptor
      rc = write(fd, buf, rc);
      if (rc <= 0)
	 break;

      if (size > 0 && br >= size) {
	 rc = 0;
	 break;
      }
   }
   free(buf);
   return (int)rc;
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(const char *method, const char *path, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source) {
   return priv->sendHTTPMessage(0, 0, method, path, http_version, headers, data, size, source);
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(QoreHashNode *info, const char *method, const char *path, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source) {
   return priv->sendHTTPMessage(0, info, method, path, http_version, headers, data, size, source);
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode *info, const char *method, const char *path, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source) {
   return priv->sendHTTPMessage(xsink, info, method, path, http_version, headers, data, size, source);
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode *info, const char *method, const char *path, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source, int timeout_ms) {
   return priv->sendHTTPMessage(xsink, info, method, path, http_version, headers, data, size, source, timeout_ms);
}

// returns 0 for success
int QoreSocket::sendHTTPResponse(int code, const char *desc, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source) {
   return priv->sendHTTPResponse(0, code, desc, http_version, headers, data, size, source);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char *desc, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source) {
   return priv->sendHTTPResponse(xsink, code, desc, http_version, headers, data, size, source);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char *desc, const char *http_version, const QoreHashNode *headers, const void *data, qore_size_t size, int source, int timeout_ms) {
   return priv->sendHTTPResponse(xsink, code, desc, http_version, headers, data, size, source, timeout_ms);
}

AbstractQoreNode *QoreSocket::readHTTPHeader(int timeout, int *rc, int source) {
   assert(rc);
   qore_offset_t nrc;
   AbstractQoreNode* n = priv->readHTTPHeader(0, 0, timeout, nrc, source);
   *rc = (int)nrc;
   return n;
}

// rc is:
//    0 for remote end shutdown
//   -1 for socket error
//   -2 for socket not open
//   -3 for timeout
AbstractQoreNode *QoreSocket::readHTTPHeader(QoreHashNode *info, int timeout, int *rc, int source) {
   assert(rc);
   qore_offset_t nrc;
   AbstractQoreNode* n = priv->readHTTPHeader(0, info, timeout, nrc, source);
   *rc = (int)nrc;
   return n;
}

QoreHashNode* QoreSocket::readHTTPHeader(ExceptionSink* xsink, QoreHashNode *info, int timeout, int source) {
   assert(xsink);
   qore_offset_t rc;
   // qore_socket_private::readHTTPHeader() always returns a QoreHashNode* (or 0) if an ExceptionSink argument is passed
   return static_cast<QoreHashNode*>(priv->readHTTPHeader(xsink, info, timeout, rc, source));
}

// receive a binary message in HTTP chunked format
QoreHashNode *QoreSocket::readHTTPChunkedBodyBinary(int timeout, ExceptionSink *xsink, int source) {
   SimpleRefHolder<BinaryNode> b(new BinaryNode);
   QoreString str; // for reading the size of each chunk
   
   qore_offset_t rc;
   // read the size then read the data and append to buffer
   while (true) {
      // state = 0, nothing
      // state = 1, \r received
      int state = 0;
      while (true) {
	 char c;
	 rc = priv->recv(xsink, "readHTTPChunkedBodyBinary", &c, 1, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBodyBinary", xsink);
	    }
	    return 0;
	 }
	 
	 if (!state && c == '\r')
	    state = 1;
	 else if (state && c == '\n')
	    break;
	 else {
	    if (state) {
	       state = 0;
	       str.concat('\r');
	    }
	    str.concat(c);
	 }
      }
      // DEBUG
      //printd(0, "QoreSocket::readHTTPChunkedBodyBinary(): got chunk size ("QSD" bytes) string: %s\n", str.strlen(), str.getBuffer());

      // terminate string at ';' char if present
      char *p = (char *)strchr(str.getBuffer(), ';');
      if (p)
	 *p = '\0';
      long size = strtol(str.getBuffer(), 0, 16);
      priv->do_chunked_read(QORE_EVENT_HTTP_CHUNK_SIZE, size, str.strlen(), source);
      if (size == 0)
	 break;
      if (size < 0) {
	 xsink->raiseException("READ-HTTP-CHUNK-ERROR", "negative value given for chunk size (%ld)", size);
	 return 0;
      }

      // prepare string for chunk
      str.allocate(size + 1);

      // read chunk directly into string buffer    
      qore_offset_t bs = size < DEFAULT_SOCKET_BUFSIZE ? size : DEFAULT_SOCKET_BUFSIZE;
      qore_offset_t br = 0; // bytes received
      while (true) {
	 rc = priv->recv(xsink, "readHTTPChunkedBodyBinary", (char *)str.getBuffer() + br, bs, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBodyBinary", xsink);
	    }
	    return 0;
	 }
	 br += rc;
	 
	 if (br >= size)
	    break;
	 if (size - br < bs)
	    bs = size - br;
      }

      // copy string buffer to binary object
      b->append(str.getBuffer(), size);
      // DEBUG
      //printd(0, "QoreSocket::readHTTPChunkedBodyBinary(): received binary chunk: size=%d br="QSD" total="QSD"\n", size, br, b->size());
      
      // read crlf after chunk
      char crlf[2];
      br = 0;
      while (br < 2) {
	 rc = priv->recv(xsink, "readHTTPChunkedBodyBinary", crlf, 2 - br, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBodyBinary", xsink);
	    }
	    return 0;
	 }
	 br += rc;
      }      
      priv->do_chunked_read(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED, size, size + 2, source);

      // ensure string is blanked for next read
      str.clear();
   }

   // read footers or nothing
   QoreStringNodeHolder hdr(priv->readHTTPData(xsink, "readHTTPChunkedBodyBinary", timeout, rc, 1));
   if (!hdr) {
      assert(*xsink);
      return 0;
   }
   QoreHashNode *h = new QoreHashNode;

   //printd(0, "QoreSocket::readHTTPChunkedBodyBinary(): saving binary body: %p size=%ld\n", b->getPtr(), b->size());
   h->setKeyValue("body", b.release(), xsink);
   
   if (hdr->strlen() >= 2 && hdr->strlen() <= 4)
      return h;

   priv->convertHeaderToHash(h, (char *)hdr->getBuffer());
   priv->do_read_http_header(QORE_EVENT_HTTP_FOOTERS_RECEIVED, h, source);

   return h; 
}

// receive a message in HTTP chunked format
QoreHashNode *QoreSocket::readHTTPChunkedBody(int timeout, ExceptionSink *xsink, int source) {
   QoreStringNodeHolder buf(new QoreStringNode(priv->enc));
   QoreString str; // for reading the size of each chunk
   
   qore_offset_t rc;
   // read the size then read the data and append to buf
   while (true) {
      // state = 0, nothing
      // state = 1, \r received
      int state = 0;
      while (true) {
	 char c;
	 rc = priv->recv(xsink, "readHTTPChunkedBody", &c, 1, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBody", xsink);
	    }
	    return 0;
	 }
      
	 if (!state && c == '\r')
	    state = 1;
	 else if (state && c == '\n')
	    break;
	 else {
	    if (state) {
	       state = 0;
	       str.concat('\r');
	    }
	    str.concat(c);
	 }
      }
      // DEBUG
      //printd(0, "got chunk size ("QSD" bytes) string: %s\n", str.strlen(), str.getBuffer());

      // terminate string at ';' char if present
      char *p = (char *)strchr(str.getBuffer(), ';');
      if (p)
	 *p = '\0';
      qore_offset_t size = strtol(str.getBuffer(), 0, 16);
      priv->do_chunked_read(QORE_EVENT_HTTP_CHUNK_SIZE, size, str.strlen(), source);
      if (size == 0)
	 break;
      if (size < 0) {
	 xsink->raiseException("READ-HTTP-CHUNK-ERROR", "negative value given for chunk size (%ld)", size);
	 return 0;
      }
      // ensure string is blanked for next read
      str.clear();

      // prepare string for chunk
      buf->allocate((unsigned)(buf->strlen() + size + 1));
      
      // read chunk directly into string buffer    
      qore_offset_t bs = size < DEFAULT_SOCKET_BUFSIZE ? size : DEFAULT_SOCKET_BUFSIZE;
      qore_offset_t br = 0; // bytes received
      while (true) {
	 rc = priv->recv(xsink, "readHTTPChunkedBody", (char *)buf->getBuffer() + buf->strlen() + br, bs, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBody", xsink);
	    }
	    return 0;
	 }
	 br += rc;
	 
	 if (br >= size)
	    break;
	 if (size - br < bs)
	    bs = size - br;
      }

      // ensure new data read is included in string size
      buf->terminate(buf->strlen() + size);
      // DEBUG
      //printd(0, "got chunk ("QSD" bytes): %s\n", br, buf->getBuffer() + buf->strlen() -  size);

      // read crlf after chunk
      char crlf[2];
      br = 0;
      while (br < 2) {
	 rc = priv->recv(xsink, "readHTTPChunkedBody", crlf, 2 - br, 0, timeout, false);
	 if (rc <= 0) {
	    if (!*xsink) {
	       assert(!rc);
	       se_closed("readHTTPChunkedBody", xsink);
	    }
	    return 0;
	 }
	 br += rc;
      }
      priv->do_chunked_read(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED, size, size + 2, source);
   }

   // read footers or nothing
   QoreStringNodeHolder hdr(priv->readHTTPData(xsink, "readHTTPChunkedBody", timeout, rc, 1));
   if (!hdr) {
      assert(*xsink);
      return 0;
   }

   //printd(5, "chunked body encoding=%s\n", buf->getEncoding()->getCode());

   QoreHashNode *h = new QoreHashNode;
   h->setKeyValue("body", buf.release(), xsink);
   
   if (hdr->strlen() >= 2 && hdr->strlen() <= 4)
      return h;

   priv->convertHeaderToHash(h, (char *)hdr->getBuffer());
   priv->do_read_http_header(QORE_EVENT_HTTP_FOOTERS_RECEIVED, h, source);

   return h;
}

bool QoreSocket::isDataAvailable(int timeout) const {
   return priv->isDataAvailable(timeout, 0, 0);
}

bool QoreSocket::isWriteFinished(int timeout) const {
   return priv->isWriteFinished(timeout, 0, 0);
}

bool QoreSocket::isDataAvailable(ExceptionSink* xsink, int timeout) const {
   return priv->isDataAvailable(timeout, "isDataAvailable", xsink);
}

bool QoreSocket::isWriteFinished(ExceptionSink* xsink, int timeout) const {
   return priv->isWriteFinished(timeout, "isWriteFinished", xsink);
}

int QoreSocket::upgradeClientToSSL(X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   if (priv->ssl)
      return 0;
   return priv->upgradeClientToSSLIntern("upgradeClientToSSL", cert, pkey, xsink);
}

int QoreSocket::upgradeServerToSSL(X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   if (priv->sock == QORE_INVALID_SOCKET)
      return -1;
   if (priv->ssl)
      return 0;
   return priv->upgradeServerToSSLIntern("upgradeServerToSSL", cert, pkey, xsink);
}

/* currently hardcoded to SOCK_STREAM (tcp-only)
   if there is no port specifier, opens UNIX domain socket (if necessary)
   and binds to a local UNIX socket file
   for UNIX domain sockets: AF_UNIX
   - bind("filename");
   for ipv4 (unless an ipv6 address is detected in the host part): AF_INET
   - bind("interface:port");
   for ipv6 sockets: AF_INET6
   - bind("[interface]:port");
*/
int QoreSocket::bind(const char *name, bool reuseaddr) {
   //printd(5, "QoreSocket::bind(%s)\n", name);
   // see if there is a port specifier
   const char *p = strrchr(name, ':');
   if (p) {
      QoreString host(name, p - name);
      QoreString service(p + 1);

      // if the address is an ipv6 address like: [<addr>], then bind as ipv6
      if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
	 host.terminate(host.strlen() - 1);
	 return priv->bindINET(host.getBuffer() + 1, service.getBuffer(), reuseaddr, AF_INET6, SOCK_STREAM);
      }

      // assume an ipv6 address if there is a ':' character in the hostname, otherwise bind ipv4
      return priv->bindINET(host.getBuffer(), service.getBuffer(), reuseaddr, strchr(host.getBuffer(), ':') ? AF_INET6 : AF_INET, SOCK_STREAM);
   }

   return priv->bindUNIX(name, SOCK_STREAM, 0);
}

int QoreSocket::bindUNIX(const char *name, int socktype, int protocol, ExceptionSink *xsink) {
   return priv->bindUNIX(name, socktype, protocol, xsink);
}

int QoreSocket::bindINET(const char *name, const char *service, bool reuseaddr, int family, int socktype, int protocol, ExceptionSink *xsink) {
   return priv->bindINET(name, service, reuseaddr, family, socktype, protocol, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens INET socket and binds to a tcp port on all interfaces
// closes socket if already open, because the socket will be
// bound to all interfaces
// * bind(port);
int QoreSocket::bind(int prt, bool reuseaddr) {
   priv->close();
   QoreString service;
   service.sprintf("%d", prt);
   return priv->bindINET(0, service.getBuffer(), reuseaddr);
}

// to bind to an INET tcp port on a specific interface
int QoreSocket::bind(const char *iface, int prt, bool reuseaddr) {
   printd(5, "QoreSocket::bind(%s, %d)\n", iface, prt);
   QoreString service;
   service.sprintf("%d", prt);
   return priv->bindINET(iface, service.getBuffer(), reuseaddr);
}

// to bind an INET socket to a particular address
int QoreSocket::bind(const struct sockaddr *addr, int size) {
   // close if it's already been opened as an INET socket or with different parameters
   if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != AF_INET || priv->stype != SOCK_STREAM || priv->sprot != 0))
      close();

   // try to open socket if necessary
   if (priv->sock == QORE_INVALID_SOCKET && priv->openINET())
      return -1;

   if ((::bind(priv->sock, addr, size)) == QORE_SOCKET_ERROR) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
      // set errno from windows error
      sock_get_error();
#endif
      return -1;
   }

   // set port number to unknown
   priv->port = -1;
   //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
   return 0;   
}

int QoreSocket::bind(int family, const struct sockaddr *addr, int size, int sock_type, int protocol) {
   family = q_get_af(family);
   sock_type = q_get_sock_type(sock_type);

   // close if it's already been opened as an INET socket or with different parameters
   if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != family || priv->stype != sock_type || priv->sprot != protocol))
      close();

   // try to open socket if necessary
   if (priv->sock == QORE_INVALID_SOCKET && priv->openINET(family, sock_type, protocol))
      return -1;

   if ((::bind(priv->sock, addr, size)) == -1) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__ 
      // set errno from windows error
      sock_get_error();
#endif
      return -1;
   }

   // set port number
   int prt = q_get_port_from_addr(addr);
   priv->port = prt ? prt : -1;
   //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
   return 0;   
}

// find out what port we're connected to
int QoreSocket::getPort() {
   return priv->getPort();
}

// QoreSocket::accept()
// returns a new socket
QoreSocket *QoreSocket::accept(SocketSource *source, ExceptionSink *xsink) {
   int rc = priv->accept_internal(source, -1, xsink);
   if (rc < 0)
      return 0;

   return new QoreSocket(rc, priv->sfamily, priv->stype, priv->sprot, priv->enc);
}

// QoreSocket::acceptSSL()
// accepts a new connection, negotiates an SSL connection, and returns the new socket
QoreSocket *QoreSocket::acceptSSL(SocketSource *source, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   QoreSocket *s = accept(source, xsink);
   if (!s)
      return 0;

   if (s->priv->upgradeServerToSSLIntern("acceptSSL", cert, pkey, xsink)) {
      assert(*xsink);
      delete s;
      return 0;
   }
   
   return s;
}

// accept a connection and replace the socket with the new connection
int QoreSocket::acceptAndReplace(SocketSource *source) {
   QORE_TRACE("QoreSocket::acceptAndReplace()");
   int rc = priv->accept_internal(source);
   if (rc < 0)
      return -1;
   priv->close_internal();
   priv->sock = rc;

   return 0;
}

QoreSocket *QoreSocket::accept(int timeout_ms, ExceptionSink *xsink) {
   int rc = priv->accept_internal(0, timeout_ms, xsink);
   if (rc < 0)
      return 0;

   return new QoreSocket(rc, priv->sfamily, priv->stype, priv->sprot, priv->enc);
}

QoreSocket *QoreSocket::acceptSSL(int timeout_ms, X509 *cert, EVP_PKEY *pkey, ExceptionSink *xsink) {
   std::auto_ptr<QoreSocket> s(accept(timeout_ms, xsink));
   if (!s.get())
      return 0;

   if (s->priv->upgradeServerToSSLIntern("acceptSSL", cert, pkey, xsink)) {
      assert(*xsink);
      return 0;
   }
   
   return s.release();
}

int QoreSocket::acceptAndReplace(int timeout_ms, ExceptionSink *xsink) {
   int rc = priv->accept_internal(0, timeout_ms, xsink);
   if (rc < 0)
      return -1;

   priv->close_internal();
   priv->sock = rc;
   return 0;
}

int QoreSocket::listen() {
   return priv->listen();
}

int QoreSocket::send(const char *buf, qore_size_t size) {
   return priv->send(0, "send", buf, size);
}

int QoreSocket::send(const char *buf, qore_size_t size, ExceptionSink* xsink) {
   return priv->send(xsink, "send", buf, size);
}

int QoreSocket::send(const char *buf, qore_size_t size, int timeout_ms, ExceptionSink* xsink) {
   return priv->send(xsink, "send", buf, size, timeout_ms);
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString *msg, ExceptionSink *xsink) {
   TempEncodingHelper tstr(msg, priv->enc, xsink);
   if (!tstr)
      return -1;

   return priv->send(xsink, "send", (const char *)tstr->getBuffer(), tstr->strlen());
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString *msg, int timeout_ms, ExceptionSink *xsink) {
   TempEncodingHelper tstr(msg, priv->enc, xsink);
   if (!tstr)
      return -1;

   return priv->send(xsink, "send", (const char *)tstr->getBuffer(), tstr->strlen(), timeout_ms);
}

int QoreSocket::send(const BinaryNode *b) {
   return priv->send(0, "send", (char *)b->getPtr(), b->size());
}

int QoreSocket::send(const BinaryNode *b, ExceptionSink* xsink) {
   return priv->send(xsink, "send", (char *)b->getPtr(), b->size());
}

int QoreSocket::send(const BinaryNode *b, int timeout_ms, ExceptionSink* xsink) {
   return priv->send(xsink, "send", (char *)b->getPtr(), b->size(), timeout_ms);
}

int QoreSocket::setSendTimeout(int ms) {
   struct timeval tv;
   tv.tv_sec  = ms / 1000;
   tv.tv_usec = (ms % 1000) * 1000;

   return setsockopt(priv->sock, SOL_SOCKET, SO_SNDTIMEO, (SETSOCKOPT_ARG_4)&tv, sizeof(struct timeval));
}

int QoreSocket::setRecvTimeout(int ms) {
   struct timeval tv;
   tv.tv_sec  = ms / 1000;
   tv.tv_usec = (ms % 1000) * 1000;

   return setsockopt(priv->sock, SOL_SOCKET, SO_RCVTIMEO, (SETSOCKOPT_ARG_4)&tv, sizeof(struct timeval));
}

int QoreSocket::getSendTimeout() const {
   return priv->getSendTimeout();
}

int QoreSocket::getRecvTimeout() const {
   return priv->getRecvTimeout();
}

void QoreSocket::setEventQueue(Queue *cbq, ExceptionSink *xsink) {
   priv->setEventQueue(cbq, xsink);
}

Queue *QoreSocket::getQueue() {
   return priv->cb_queue;
}

void QoreSocket::cleanup(ExceptionSink *xsink) {
   priv->cleanup(xsink);
}

int64 QoreSocket::getObjectIDForEvents() const {
   return priv->getObjectIDForEvents();
}

QoreHashNode *QoreSocket::getPeerInfo(ExceptionSink *xsink) const {
   return priv->getPeerInfo(xsink);
}

QoreHashNode *QoreSocket::getSocketInfo(ExceptionSink *xsink) const {
   return priv->getSocketInfo(xsink);
}

void QoreSocket::setAccept(QoreObject *o) {
   priv->setAccept(o);
}

/*
 * This stl "compatability" strstream implementation is
 * included with shore for use with newer c++ compilers whic
 * do not provide the strstream functionality.
 *
 * stringstreams are not usable for the functions shore needs,
 * since they provide no way of writing to and reading from
 * memory objects.
 *
 * This file should not be changed, except to incorporate bug
 * fixes from the SGI STL code.
 */

/*
 * Copyright (c) 1998
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */ 

// Implementation of the classes in header <strstream>.
// WARNING: The classes defined in <strstream> are DEPRECATED.  This
// header is defined in section D.7.1 of the C++ standard, and it
// MAY BE REMOVED in a future standard revision.  You should use the
// header <sstream> instead.

#include <w_compat_strstream.h>
#include <algorithm>
#include <new>
#include <cstdlib>
#include <cstring>
#include <climits>


namespace shore_compat {

using namespace std;

// strstreambuf constructor, destructor.

strstreambuf::strstreambuf(streamsize initial_capacity)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(true),
    _M_constant(false)
{
  streamsize n = max(initial_capacity, streamsize(16));

  char* buf = _M_alloc(n);
  if (buf) {
    setp(buf, buf + n);
    setg(buf, buf, buf);
  }
}

strstreambuf::strstreambuf(void* (*alloc_f)(size_t), void (*free_f)(void*))
  : _Base(),
    _M_alloc_fun(alloc_f), _M_free_fun(free_f),
    _M_dynamic(true), 
    _M_constant(false)
{
  streamsize n = 16;

  char* buf = _M_alloc(n);
  if (buf) {
    setp(buf, buf + n);
    setg(buf, buf, buf);
  }
}

strstreambuf::strstreambuf(char* get, streamsize n, char* put)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(false)
{
  _M_setup(get, put, n);
}

strstreambuf::strstreambuf(signed char* get, streamsize n, signed char* put)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(false)
{
  _M_setup(reinterpret_cast<char*>(get), reinterpret_cast<char*>(put), n);
}

strstreambuf::strstreambuf(unsigned char* get, streamsize n,
                           unsigned char* put)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(false)
{
  _M_setup(reinterpret_cast<char*>(get), reinterpret_cast<char*>(put), n);
}

strstreambuf::strstreambuf(const char* get, streamsize n)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(true)
{
  _M_setup(const_cast<char*>(get), 0, n);
}

strstreambuf::strstreambuf(const signed char* get, streamsize n)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(true)
{
  _M_setup(reinterpret_cast<char*>(const_cast<signed char*>(get)), 0, n);
}

strstreambuf::strstreambuf(const unsigned char* get, streamsize n)
  : _Base(),
    _M_alloc_fun(0), _M_free_fun(0),
    _M_dynamic(false), 
    _M_constant(true)
{
  _M_setup(reinterpret_cast<char*>(const_cast<unsigned char*>(get)), 0, n);
}

strstreambuf::~strstreambuf()
{
  if (_M_dynamic 
          )
    _M_free(eback());    
}

char* strstreambuf::str()
{
  return eback();
}

int strstreambuf::pcount() const
{
  return pptr() ? pptr() - pbase() : 0;
}

strstreambuf::int_type strstreambuf::overflow(int_type c) {
  if (c == traits_type::eof())
    return traits_type::not_eof(c);

  // Try to expand the buffer.
  if (pptr() == epptr() && _M_dynamic 
          && !_M_constant) {
    ptrdiff_t old_size = epptr() - pbase();
    ptrdiff_t new_size = max(2 * old_size, ptrdiff_t(1));

    char* buf = _M_alloc(new_size);
    if (buf) {
      memcpy(buf, pbase(), old_size);

      char* old_buffer = pbase();
      bool reposition_get = false;
      ptrdiff_t old_get_offset;
      if (gptr() != 0) {
        reposition_get = true;
        old_get_offset = gptr() - eback();
      }

      setp(buf, buf + new_size);
      pbump(old_size);

      if (reposition_get) 
        setg(buf, buf + old_get_offset, buf + max(old_get_offset, old_size));

      _M_free(old_buffer);
    }
  }

  if (pptr() != epptr()) {
    *pptr() = c;
    pbump(1);
    return c;
  }
  else
    return traits_type::eof();
}

strstreambuf::int_type strstreambuf::pbackfail(int_type c)
{
  if (gptr() != eback()) {
    if (c == _Traits::eof()) {
      gbump(-1);
      return _Traits::not_eof(c);
    }
    else if (c == gptr()[-1]) {
      gbump(-1);
      return c;
    }
    else if (!_M_constant) {
      gbump(-1);
      *gptr() = c;
      return c;
    }
  }

  return _Traits::eof();
}

strstreambuf::int_type strstreambuf::underflow()
{
  if (gptr() == egptr() && pptr() && pptr() > egptr())
    setg(eback(), gptr(), pptr());

  if (gptr() != egptr())
    return (unsigned char) *gptr();
  else
    return _Traits::eof();
}

basic_streambuf<char, char_traits<char> >* 
strstreambuf::setbuf(char*, streamsize)
{
  return this;
}

strstreambuf::pos_type
strstreambuf::seekoff(off_type off,
                      ios_base::seekdir dir, ios_base::openmode mode)
{
  bool do_get = false;
  bool do_put = false;

  if ((mode & (ios_base::in | ios_base::out)) ==
          (ios_base::in | ios_base::out) &&
      (dir == ios_base::beg || dir == ios_base::end))
    do_get = do_put = true;
  else if (mode & ios_base::in)
    do_get = true;
  else if (mode & ios_base::out)
    do_put = true;

  // !gptr() is here because, according to D.7.1 paragraph 4, the seekable
  // area is undefined if there is no get area.
  if ((!do_get && !do_put) || (do_put && !pptr()) || !gptr())
    return pos_type(off_type(-1));

  char* seeklow  = eback();
  char* seekhigh = epptr() ? epptr() : egptr();

  off_type newoff;
  switch(dir) {
  case ios_base::beg:
    newoff = 0;
    break;
  case ios_base::end:
    newoff = seekhigh - seeklow;
    break;
  case ios_base::cur:
    newoff = do_put ? pptr() - seeklow : gptr() - seeklow;
    break;
  default:
    return pos_type(off_type(-1));
  }

  off += newoff;
  if (off < 0 || off > seekhigh - seeklow)
    return pos_type(off_type(-1));

  if (do_put) {
    if (seeklow + off < pbase()) {
      setp(seeklow, epptr());
      pbump(off);
    }
    else {
      setp(pbase(), epptr());
      pbump(off - (pbase() - seeklow));
    }
  }
  if (do_get) {
    if (off <= egptr() - seeklow)
      setg(seeklow, seeklow + off, egptr());
    else if (off <= pptr() - seeklow)
      setg(seeklow, seeklow + off, pptr());
    else
      setg(seeklow, seeklow + off, epptr());
  }

  return pos_type(newoff);
}

strstreambuf::pos_type
strstreambuf::seekpos(pos_type pos, ios_base::openmode mode)
{
  return seekoff(pos - pos_type(off_type(0)), ios_base::beg, mode);
}


char* strstreambuf::_M_alloc(size_t n)
{
  if (_M_alloc_fun)
    return static_cast<char*>(_M_alloc_fun(n));
  else
    return new char[n];
}

void strstreambuf::_M_free(char* p)
{
  if (p) {
    if (_M_free_fun) {
      _M_free_fun(p);
    }
    else {
      delete[] p;
    }
  }
}

void strstreambuf::_M_setup(char* get, char* put, streamsize n)
{
  if (get) {
    size_t N = n > 0 ? size_t(n) : n == 0 ? strlen(get) : size_t(INT_MAX);
    
    if (put) {
      setg(get, get, put);
      setp(put, put + N);
    }
    else {
      setg(get, get, get + N);
    }
  }
}

//----------------------------------------------------------------------
// Class istrstream

istrstream::istrstream(char* s)
  : basic_ios<char>(), basic_istream<char>(0), _M_buf(s, 0)
{
  basic_ios<char>::init(&_M_buf);
}

istrstream::istrstream(const char* s)
  : basic_ios<char>(), basic_istream<char>(0), _M_buf(s, 0)
{
  basic_ios<char>::init(&_M_buf);
}

istrstream::istrstream(char* s, streamsize n)
  : basic_ios<char>(), basic_istream<char>(0), _M_buf(s, n)
{
  basic_ios<char>::init(&_M_buf);
}

istrstream::istrstream(const char* s, streamsize n)
  : basic_ios<char>(), basic_istream<char>(0), _M_buf(s, n)
{
  basic_ios<char>::init(&_M_buf);
}

istrstream::~istrstream() {}

strstreambuf* istrstream::rdbuf() const {
  return const_cast<strstreambuf*>(&_M_buf);
}

char* istrstream::str() { return _M_buf.str(); }

//----------------------------------------------------------------------
// Class ostrstream

ostrstream::ostrstream()
  : basic_ios<char>(), basic_ostream<char>(0), _M_buf()
{
  basic_ios<char>::init(&_M_buf);
}

ostrstream::ostrstream(char* s, int n, ios_base::openmode mode)
  : basic_ios<char>(), basic_ostream<char>(0), 
    _M_buf(s, n, mode & ios_base::app ? s + strlen(s) : s)
{
  basic_ios<char>::init(&_M_buf);
}

ostrstream::~ostrstream() {}

strstreambuf* ostrstream::rdbuf() const 
{
  return const_cast<strstreambuf*>(&_M_buf);
}

char* ostrstream::str()
{
  return _M_buf.str();
}

int ostrstream::pcount() const
{
  return _M_buf.pcount();
}

} /* namespace shore_compat */

// Local Variables:
// mode:C++
// End:

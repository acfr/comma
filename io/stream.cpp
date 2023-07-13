// This file is part of comma, a generic and flexible library
// Copyright (c) 2011 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


/// @author vsevolod vlaskine

#include <sys/stat.h>
#ifndef WIN32
#include <signal.h>
#include <sys/ioctl.h>
#else
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#endif

#include <fcntl.h>
#include <fstream>
#include <vector>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/bind/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "../base/exception.h"
#include "../string/string.h"
#include "file_descriptor.h"
#include "select.h"
#include "stream.h"

#ifdef USE_ZEROMQ
#include "zeromq/stream.h"
#endif

namespace comma { namespace io {

namespace impl {

template < typename S >
struct traits {};

template <>
struct traits < std::istream >
{
    typedef std::ifstream file_stream;
    static bool is_standard( const std::istream* is ) { return is == &std::cin; }
    static std::istream* standard( comma::io::mode::value mode )
    {
        #ifdef WIN32
        if( mode == comma::io::mode::binary ) { _setmode( _fileno( stdin ), _O_BINARY ); }
        #endif
        return &std::cin;
    }
    static comma::io::file_descriptor standard_fd()
    {
        #ifdef WIN32
        return io::invalid_file_descriptor;
        #else
        return 0;
        #endif
    }
    #ifdef WIN32
    static io::file_descriptor open( const std::string& name ) { return io::invalid_file_descriptor; }
    #else
    static io::file_descriptor open( const std::string& name ) { return ::open( &name[0], O_RDONLY | O_NONBLOCK ); }
    #endif
};

template <>
struct traits < std::ostream >
{
    typedef std::ofstream file_stream;
    static bool is_standard( const std::ostream* is ) { return is == &std::cout || is == &std::cerr; }
    static std::ostream* standard( comma::io::mode::value mode )
    {
        #ifdef WIN32
        if( mode == comma::io::mode::binary ) { ( _fileno( stdout ), _O_BINARY ); }
        #endif
        return &std::cout;
    }
    static comma::io::file_descriptor standard_fd()
    {
        #ifdef WIN32
        return io::invalid_file_descriptor;
        #else
        return 1;
        #endif
    }
    #ifdef WIN32
        #ifdef O_LARGEFILE
            static io::file_descriptor open( const std::string& name ) { return _open( &name[0], O_WRONLY | O_CREAT | O_LARGEFILE, _S_IWRITE ); }
        #else
            static io::file_descriptor open( const std::string& name ) { return _open( &name[0], O_WRONLY | O_CREAT, _S_IWRITE ); }
        #endif
    #else
        #ifdef O_LARGEFILE
            static io::file_descriptor open( const std::string& name ) { return ::open( &name[0], O_WRONLY | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_LARGEFILE ); }
        #else
            static io::file_descriptor open( const std::string& name ) { return ::open( &name[0], O_WRONLY | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ); }
        #endif
    #endif
};

template <>
struct traits < std::iostream >
{
    typedef std::fstream file_stream; // quick and dirty, does not matter for now
    static bool is_standard( const std::iostream* ) { return false; }
    static std::iostream* standard( comma::io::mode::value mode ) { (void) mode; return NULL; }
    static comma::io::file_descriptor standard_fd() { return comma::io::invalid_file_descriptor; }
    #ifdef WIN32
        static io::file_descriptor open( const std::string& name ) { return io::invalid_file_descriptor; }
    #else
        #ifdef O_LARGEFILE
            static io::file_descriptor open( const std::string& name ) { return ::open( &name[0], O_RDWR | O_NONBLOCK | O_LARGEFILE ); }
        #else
            static io::file_descriptor open( const std::string& name ) { return ::open( &name[0], O_RDWR | O_NONBLOCK ); }
        #endif
    #endif
};

template < typename S > void close_file_stream( typename traits< S >::file_stream* s, int fd )
{
    if( s ) { s->close(); }
    if( fd != io::invalid_file_descriptor ) { ::close( fd ); }
}

} // namespace impl

template < typename S >
stream< S >::~stream()
{
    if( stream_ == NULL || impl::traits< S >::is_standard( stream_ ) ) { return; }
    delete stream_;
    stream_ = NULL;
    close_ = NULL;
}

template < typename S >
S* stream< S >::lazily_make_stream_()
{
    #ifndef WIN32
    if( stream_ == NULL ) // quick and dirty: if fstream, cannot open on construction, as pipe might block
    {
        if( !boost::filesystem::is_regular_file( name_ ) && !blocking_ ) // quick and dirty
        {
            io::select select;
            select.read().add( fd_ ); // todo: express via traits
            select.write().add( fd_ ); // todo: express via traits
            select.check(); //if( !select.check() ) { return NULL; }
            if( !select.read().ready( fd_ ) && !select.write().ready( fd_ ) ) { return NULL; }
        }
        typename impl::traits< S >::file_stream* s = new typename impl::traits< S >::file_stream( name_.c_str(), static_cast< std::ios::openmode >( mode_ ) );
        if( s->bad() ) { COMMA_THROW( comma::exception, "failed to open " << name_ ); }
        stream_ = s;
        close_ = boost::bind( &impl::close_file_stream< S >, s, fd_ );
    }
    #endif // #ifndef WIN32
    return stream_;
}

template < typename S > void stream< S >::close() { close_d = true; if( close_ ) { close_(); } }

template < typename S > S* stream< S >::operator()() { return this->operator->(); }

template < typename S > S& stream< S >::operator*() { return *this->operator->(); }

template < typename S > const S* stream< S >::operator()() const { return this->operator->(); }

template < typename S > const S& stream< S >::operator*() const { return *this->operator->(); }

template < typename S > S* stream< S >::operator->() { return lazily_make_stream_(); }

template < typename S > const S* stream< S >::operator->() const { return const_cast< stream< S >& >( *this ).lazily_make_stream_(); }

static void set_non_blocking_flags_( io::file_descriptor fd )
{
#ifndef WIN32
    std::size_t flags = ::fcntl( fd, F_GETFL, 0 );
    flags = flags & ( ~O_NONBLOCK );
    ::fcntl( fd, F_SETFL, flags );
#endif // #ifndef WIN32
}

template < typename S >
comma::io::file_descriptor stream< S >::fd() const
{
    #ifdef WIN32
    if( fd_ == io::invalid_file_descriptor )
    {
        COMMA_THROW( comma::exception, "an attempt to use invalid file descriptor of: " << name_ << std::endl
                                    << "you probably are trying to use file descriptor of" << std::endl
                                    << "a regular file for select on Windows, which is not" << std::endl
                                    << "supported on Windows" << std::endl );
    }
    #endif // #ifdef WIN32
    return fd_;
}

template < typename S >
std::size_t stream< S >::available_on_file_descriptor() const
{
    int count = 0;
#ifdef WIN32
    COMMA_THROW( comma::exception, "not implemented" );
#else
    int error = ::ioctl( fd_, FIONREAD, &count );
    if( error != 0 ) { COMMA_THROW( comma::exception, "ioctl failed with error code: \"" << error << "\"" ); }
#endif
    return std::max( count, 0 );
}

template < typename S > const std::string& stream< S >::name() const { return name_; }

template < typename S >
stream< S >::stream( const std::string& name, mode::value m, mode::blocking_value blocking )
    : name_( name )
    , mode_( m )
    , stream_( NULL )
    , fd_( comma::io::invalid_file_descriptor )
    , close_d( false )
    , blocking_( blocking )
{
    std::vector< std::string > v = comma::split( name, ':' );
    if( v[0] == "tcp" )
    {
        if( v.size() != 3 ) { COMMA_THROW( comma::exception, "expected tcp:<address>:<port>, got \"" << name << "\"" ); }
#if (BOOST_VERSION >= 106600)
        boost::asio::io_context service;
#else
        boost::asio::io_service service;
#endif
        boost::asio::ip::tcp::resolver resolver( service );
        boost::asio::ip::tcp::resolver::query query( v[1] == "localhost" ? "127.0.0.1" : v[1], v[2] );
        boost::asio::ip::tcp::resolver::iterator it = resolver.resolve( query );
        boost::asio::ip::tcp::iostream* s = new boost::asio::ip::tcp::iostream( it->endpoint() );
        if( !*s ) { delete s; COMMA_THROW( comma::exception, "failed to connect to " << name << ( blocking_ ? " (todo: implement blocking mode)" : "" ) ); }
        close_ = boost::bind( &boost::asio::ip::tcp::iostream::close, s );
        // todo: make unidirectional
#if (BOOST_VERSION >= 106600)
        fd_ = s->rdbuf()->native_handle();
#else
        fd_ = s->rdbuf()->native();
#endif
        stream_ = s;
    }
    else if( v[0] == "udp" )
    {
        COMMA_THROW( comma::exception, "todo" );
    }
    else if( v[0] == "serial" )
    {
        COMMA_THROW( comma::exception, "todo" );
        /* there's no boost implementation that creates an iostream from serial port
         * (perhaps too many modes of operation, difficult to parametrize?)
         * 
         * to implement our own we need to make a stream buffer:
         *     class serial_buf : public boost::asio::streambuf {
         *          boost::asio::io_service service;
         *          boost::asio::serial_port port;
         *     }
         *     serial_buf() : connect to port
         *          needs filename, baudrate, character size, stop bit, parity
         * 
         *     serial_buf::overflow() : writing when output buffer is full
         *          write contents of buffer to port
         *     serial_buf::underflow() : reading when input buffer is empty
         *          attempt to read port into buffer
         *     serial_buf::sync() : sync buffer and serial port
         *          write contents of output buffer to port
         *          clear input buffer
         * 
         *  Then here:
         *     serial_buf = comma::io::serial_buf(...);
         *     stream_ = new S(serial_buf)
         * 
         *  Functions in serial_buf to expose these:
         *     fd_ = serial_buf.port.native_handle()
         *     close_ = serial_buf.port.close()
         *
         *  That should work (for my current use case)! Then cleanup, error handling, etc.
         *  What other modes are there?
         */
    }
#ifndef WIN32
    else if( v[0] == "local" )
    {
        boost::asio::local::stream_protocol::iostream* ls = new boost::asio::local::stream_protocol::iostream( boost::asio::local::stream_protocol::endpoint( v[1] ) );
        if( !( *ls ) ) { COMMA_THROW( comma::exception, "failed to open " << name_ << ( blocking_ ? " (todo: implement blocking)" : "" ) ); }
        close_ = boost::bind( &boost::asio::local::stream_protocol::iostream::close, ls );
        // todo: make unidirectional
#if (BOOST_VERSION >= 106600)
        fd_ = ls->rdbuf()->native_handle();
#else
        fd_ = ls->rdbuf()->native();
#endif
        stream_ = ls;
    }
#endif
#ifdef USE_ZEROMQ
    else if( v[0] == "zero-local" || v[0] == "zmq-local" )
    {
        stream_ = zeromq::make_stream< S >( "ipc://" + v[1], fd_ );
    }
    else if( v[0] == "zero-tcp" || v[0] == "zmq-tcp" )
    {
        if( v.size() != 3 ) { COMMA_THROW( comma::exception, "expected zero-tcp:<address>:<port>, got \"" << name << "\"" ); }
        stream_ = zeromq::make_stream< S >( "tcp://" + v[1] + ":" + v[2], fd_ );
    }
#endif
    else if( name == "-" )
    {
        stream_ = impl::traits< S >::standard( m );
        fd_ = impl::traits< S >::standard_fd();
    }
    else
    {
#ifdef WIN32
        typename impl::traits< S >::file_stream* s =
            m == comma::io::mode::binary ? new typename impl::traits< S >::file_stream( &name[0], std::ios::binary )
                                         : new typename impl::traits< S >::file_stream( &name[0] );
        if( s->bad() || !s->is_open() ) { COMMA_THROW( comma::exception, "failed to open " << name_ ); }
        stream_ = s;
        close_ = boost::bind( &impl::close_file_stream< S >, s, fd_ );
        fd_ = io::invalid_file_descriptor; // as select does not work on regular files on windows
#else // #ifdef WIN32
        // this is a very quick and dirty fix, since STL omits file descriptors
        // as an implementation detail, but does not provide any explicit concept
        // for sensing change on a file stream (e.g. calling select() on it)
        //
        // select std::ifstream would not work anyway, because select on a file
        // always returns immediately; however if one would like to use select()
        // on named pipes, which otherwise look like files, he will need a file
        // descriptor
        //
        // extracting file descriptor for files is implemented here:
        // http://www.ginac.de/~kreckel/fileno/
        // however, the author laments that it is a hack, etc...
        //
        // a cleaner solution, though, seems to be:
        //
        // - implement select taking selectable objects without the notion
        //   of file descriptor (which is almost done in io::select, just
        //   need to add traits there); then select on std::ifstream simply
        //   will make select returning immediately, without registering fd
        //
        // - implement proper classes for named pipes deriving from
        //   std::istream/std::fstream and make them selectable
        //
        //   currently, we simply go for a dirty trick below, which we
        //   have been using successfully in a few applications

        fd_ = impl::traits< S >::open( name );
        if( fd_ != io::invalid_file_descriptor ) { set_non_blocking_flags_( fd_ ); return; }
        if( boost::filesystem::is_regular_file( name ) ) { COMMA_THROW( comma::exception, "failed to open \"" << name << "\"" ); }
        #endif // #ifdef WIN32
    }
}

template class stream< std::istream >;
template class stream< std::ostream >;
template class stream< std::iostream >;

istream::istream( const std::string& name, mode::value mode, mode::blocking_value blocking ) : stream< std::istream >( name, mode, blocking ) {}
istream::istream( std::istream* s, io::file_descriptor fd, mode::value mode, boost::function< void() > close ) : stream< std::istream >( s, fd, mode, mode::non_blocking, close ) {}
istream::istream( std::istream* s, io::file_descriptor fd, mode::value mode, mode::blocking_value blocking, boost::function< void() > close ) : stream< std::istream >( s, fd, mode, blocking, close ) {}
ostream::ostream( const std::string& name, mode::value mode, mode::blocking_value blocking ) : stream< std::ostream >( name, mode, blocking ) {}
ostream::ostream( std::ostream* s, io::file_descriptor fd, mode::value mode, boost::function< void() > close ) : stream< std::ostream >( s, fd, mode, mode::non_blocking, close ) {}
ostream::ostream( std::ostream* s, io::file_descriptor fd, mode::value mode, mode::blocking_value blocking, boost::function< void() > close ) : stream< std::ostream >( s, fd, mode, blocking, close ) {}
iostream::iostream( const std::string& name, mode::value mode , mode::blocking_value blocking ) : stream< std::iostream >( name, mode, blocking ) {}

} } // namespace comma { namespace io {

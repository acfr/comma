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


/// @author cedric wohlleber

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/bind/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include "../../base/exception.h"
#include "../../io/file_descriptor.h"
#include "../../string/string.h"
#include "publisher.h"

namespace comma { namespace io { namespace impl {

class file_acceptor : public acceptor
{
    public:
        file_acceptor( const std::string& name, io::mode::value mode )
            : name_( name )
            , mode_( mode )
            , closed_( true )
            , fd_( io::invalid_file_descriptor )
        {
        }

        ~file_acceptor()
        {
#ifndef WIN32
            ::close( fd_ );
#else
            _close( fd_ );
#endif
        }

        io::ostream* accept( boost::posix_time::time_duration )
        {
            if( !closed_ ) { return NULL; }
#ifndef WIN32
            fd_ = ::open( &name_[0], O_WRONLY | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ); // quick and dirty
#else
            fd_ = _open( &name_[0], O_WRONLY | _O_CREAT, _S_IWRITE );
#endif
            if( fd_ == io::invalid_file_descriptor ) { return NULL; }
            closed_ = false;
            return new io::ostream( name_, mode_, io::mode::non_blocking ); // quick and dirty
        }

        void notify_closed() { closed_ = true; ::close( fd_ ); }
        
        io::file_descriptor fd() const { return fd_; }

    private:
        const std::string name_;
        const io::mode::value mode_;
        bool closed_;
        io::file_descriptor fd_; // todo: make io::ostream non-throwing on construction
};

struct Tcp {};
template < typename S > struct socket_traits {};

template <> struct socket_traits< Tcp >
{
    typedef boost::asio::ip::tcp::endpoint endpoint_type;
    typedef boost::asio::ip::tcp::acceptor acceptor;
    typedef boost::asio::ip::tcp::iostream iostream;
    typedef unsigned short name_type;
    static endpoint_type endpoint( unsigned short port ) { return endpoint_type( boost::asio::ip::tcp::v4(), port ); }
};

#ifndef WIN32
struct local {};
template <> struct socket_traits< local >
{
    typedef boost::asio::local::stream_protocol::endpoint endpoint_type;
    typedef boost::asio::local::stream_protocol::acceptor acceptor;
    typedef boost::asio::local::stream_protocol::iostream iostream;
    typedef std::string name_type;
    static endpoint_type endpoint( const std::string& name ) { return endpoint_type( name ); }
};
#endif

template < typename S >
class socket_acceptor : public acceptor
{
    public:
        socket_acceptor( const typename socket_traits< S >::name_type& name, io::mode::value mode )
            : mode_( mode )
            , acceptor_( m_service, socket_traits< S >::endpoint( name ) )
        {
#ifndef WIN32
#if (BOOST_VERSION >= 106600)
            select_.read().add( acceptor_.native_handle() );
#else
            select_.read().add( acceptor_.native() );
#endif
#else
#if (BOOST_VERSION >= 106600)
            SOCKET socket = acceptor_.native_handle();
#else
            SOCKET socket = acceptor_.native();
#endif
            select_.read().add( socket );
#endif
        }

        io::ostream* accept( boost::posix_time::time_duration timeout )
        {
            select_.wait( timeout );
#ifndef WIN32
#if (BOOST_VERSION >= 106600)
            if( !select_.read().ready( acceptor_.native_handle() ) ) { return NULL; }
#else
            if( !select_.read().ready( acceptor_.native() ) ) { return NULL; }
#endif
#else
#if (BOOST_VERSION >= 106600)
            SOCKET socket = acceptor_.native_handle();
#else
            SOCKET socket = acceptor_.native();
#endif
            if( !select_.read().ready( socket ) ) { return NULL; }
#endif
            typename socket_traits< S >::iostream* stream = new typename socket_traits< S >::iostream;
            acceptor_.accept( *( stream->rdbuf() ) );
#if (BOOST_VERSION >= 106600)
            return new io::ostream( stream, stream->rdbuf()->native_handle(), mode_, boost::bind( &socket_traits< S >::iostream::close, stream ) );
#else
            return new io::ostream( stream, stream->rdbuf()->native(), mode_, boost::bind( &socket_traits< S >::iostream::close, stream ) );
#endif
        }

        void close() { acceptor_.close(); }

#ifndef WIN32
#if (BOOST_VERSION >= 106600)
        io::file_descriptor fd() const { return const_cast< typename socket_traits< S >::acceptor& >( acceptor_ ).native_handle(); }
#else
        io::file_descriptor fd() const { return const_cast< typename socket_traits< S >::acceptor& >( acceptor_ ).native(); }
#endif
#else
        io::file_descriptor fd() const { return io::invalid_file_descriptor; }
#endif

    private:
        io::mode::value mode_;
        io::select select_;
#if (BOOST_VERSION >= 106600)
        boost::asio::io_context m_service;
#else
        boost::asio::io_service m_service;
#endif
        typename socket_traits< S >::acceptor acceptor_;
};

class zero_acceptor_ : public acceptor
{
    public:
        zero_acceptor_( const std::string& name, io::mode::value mode ):
            stream_( new io::ostream( name, mode ) ),
            accepted_( false )
        {
        }

        io::ostream* accept( boost::posix_time::time_duration )
        {
            if( accepted_ ) { return NULL; }
            accepted_ = true;
            return stream_;
        }

        void close() { stream_->close(); }
        
        io::file_descriptor fd() const { return io::invalid_file_descriptor; } // quick and dirty

    private:
        io::ostream* stream_;
        bool accepted_;
};

publisher::publisher( const std::string& name, io::mode::value mode, bool blocking, bool flush )
    : blocking_( blocking ),
      flush_( flush )
{
    std::vector< std::string > v = comma::split( name, ':' );
    if( v[0] == "tcp" )
    {
        if( v.size() != 2 ) { COMMA_THROW( comma::exception, "expected tcp server endpoint, got " << name ); }
        acceptor_.reset( new socket_acceptor< Tcp >( boost::lexical_cast< unsigned short >( v[1] ), mode ) );
    }
    else if( v[0] == "udp" )
    {
        COMMA_THROW( comma::exception, "udp: todo" );
    }
    else if( v[0] == "local" )
    {
#ifndef WIN32
        if( v.size() != 2 ) { COMMA_THROW( comma::exception, "expected local socket, got " << name ); }
        acceptor_.reset( new socket_acceptor< local >( v[1], mode ) );
#endif
    }
    else if( v[0].substr( 0, 4 ) == "zero" )
    {
        acceptor_.reset( new zero_acceptor_( name, mode ) );
    }
    else
    {
        if( name == "-" )
        {
            streams_.insert( boost::shared_ptr< io::ostream >( new io::ostream( name, mode ) ) );
#ifndef WIN32
            select_.write().add( 1 );
#endif
        }
        else
        {
            acceptor_.reset( new file_acceptor( name, mode ) );
            io::ostream* s = acceptor_->accept( boost::posix_time::time_duration() );
            streams_.insert( boost::shared_ptr< io::ostream >( s ) ); // todo: should we simply abolish file_acceptor and do it in the same way as for stdout?
            if( s->fd() == comma::io::invalid_file_descriptor ) { COMMA_THROW( comma::exception, "failed to open '" << name << "'" ); }
#ifndef WIN32
            select_.write().add( s->fd() );
#endif
        }
    }
}

unsigned int publisher::write( const char* buf, std::size_t size, bool do_accept )
{
    if( do_accept ) { accept(); }
    if( !blocking_ ) { select_.check(); } // todo: if slow, put all the files in one select
    unsigned int count = 0;
    for( streams::iterator i = streams_.begin(); i != streams_.end(); )
    {
        streams::iterator it = i++;
        if( !blocking_ && !select_.write().ready( **it ) ) { continue; }
        ( **it )->write( buf, size );
        if( flush_ ) { ( **it )->flush(); }
        if( ( **it )->good() ) { ++count; }
        else { remove_( it ); }
    }
    return count;
}

void publisher::close()
{
    if( acceptor_ ) { acceptor_->close(); }
    while( streams_.begin() != streams_.end() ) { remove_( streams_.begin() ); }
}

unsigned int publisher::accept()
{
    if( !acceptor_ ) { return 0; }
    unsigned int count = 0;
    while( true ) // while( streams_.size() < maxSize ?
    {
        io::ostream* s = acceptor_->accept();
        if( s == NULL ) { return count; }
        streams_.insert( boost::shared_ptr< io::ostream >( s ) );
        select_.write().add( *s );
        ++count;
    }
}

void publisher::remove_( streams::iterator it )
{
    select_.write().remove( **it );
    ( *it )->close();
    if( acceptor_ ) { acceptor_->notify_closed(); }
    streams_.erase( it );
}

std::size_t publisher::size() const { return streams_.size(); }

} } } // namespace comma { namespace io { namespace impl {

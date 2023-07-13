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

/// @authors cedric wohlleber, vsevolod vlaskine, dave jennings

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include "../../application/contact_info.h"
#include "../../application/command_line_options.h"
#include "../../application/signal_flag.h"
#include "../../base/last_error.h"
#include "../../io/file_descriptor.h"
#include "../../io/publisher.h"
#include "../../string/string.h"
#include "../../sync/synchronized.h"

//#include <google/profiler.h>

static void usage( bool verbose = false )
{
    std::cerr << std::endl;
    std::cerr << "read from standard input and write to given outputs (files, sockets, named pipes):" << std::endl;
    std::cerr << std::endl;
    std::cerr << "- the data is only written to the outputs that are ready for writing" << std::endl;
    std::cerr << "- client can connect and disconnect at any time" << std::endl;
    std::cerr << "- only full packets are written" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: io-publish [<options>] <outputs>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "general options" << std::endl;
    std::cerr << "    --help,-h: show this help" << std::endl;
    std::cerr << "    --verbose,-v: more output to stderr" << std::endl;
    std::cerr << std::endl;
    std::cerr << "stream options" << std::endl;
    std::cerr << "    --size,-s: binary input; packet size" << std::endl;
    std::cerr << "    --multiplier,-m: multiplier for packet size, default is 1. The actual packet size will be m * s" << std::endl;
    std::cerr << "    --no-discard: if present, do blocking write to every open stream" << std::endl;
    std::cerr << "    --no-flush: if present, do not flush the output stream (use on high bandwidth sources)" << std::endl;
    std::cerr << "    --exec=[<cmd>]: read from cmd rather than stdin" << std::endl;
    std::cerr << "    -- [<cmd>]: alternate syntax for specifying a command (simplifies quoting)" << std::endl;
    std::cerr << "    --on-demand: only run <cmd> when a client is connected" << std::endl;
    std::cerr << std::endl;
    std::cerr << "client options" << std::endl;
    std::cerr << "    --exit-on-no-clients,-e: once the last client disconnects, exit" << std::endl;
    std::cerr << "    --output-number-of-clients,--clients: output to stdout timestamped number of clients whenever it changes" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    attention: in the current implementation, the number of clients will be" << std::endl;
    std::cerr << "               updated only on attempt to write a new record," << std::endl;
    std::cerr << "               i.e. output number of clients will not change if there are no new" << std::endl;
    std::cerr << "               records on stdin, even if the actual number of clients changes" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    known problems: io::ostream or at least boost::asio::ostream does not mark" << std::endl;
    std::cerr << "               stream as bad, if one tries to write to it first time after" << std::endl;
    std::cerr << "               stream has been closed; the stream is marked as bad only after" << std::endl;
    std::cerr << "               writing to it second time." << std::endl;
    std::cerr << "               This problem is pretty benign: the worst thing that happens is" << std::endl;
    std::cerr << "               writing to a closed stream, which will not cause grief unless you" << std::endl;
    std::cerr << "               specifically rely on io-publish exiting on no clients for a" << std::endl;
    std::cerr << "               rarely sent heartbeat." << std::endl;
    std::cerr << std::endl;
    std::cerr << "               io-publish will not be very responsive in counting clients for" << std::endl;
    std::cerr << "               low bandwidth streams. It immediately recognises new clients" << std::endl;
    std::cerr << "               but might take a while to notice that a client has gone." << std::endl;
    std::cerr << "               This affects --output-number-of-clients and --on-demand." << std::endl;
    std::cerr << std::endl;
    std::cerr << "output streams" << std::endl;
    std::cerr << "    tcp:<port>: e.g. tcp:1234" << std::endl;
    std::cerr << "    udp:<port>: e.g. udp:1234 (todo)" << std::endl;
    std::cerr << "    local:<name>: linux/unix local server socket e.g. local:./tmp/my_socket" << std::endl;
    std::cerr << "    <named pipe name>: named pipe, which will be re-opened, if client reconnects" << std::endl;
    std::cerr << "    <filename>: a regular file" << std::endl;
    std::cerr << std::endl;
    std::cerr << "examples" << std::endl;
    std::cerr << "    cat data | io-publish tcp:1234 --size 100" << std::endl;
    std::cerr << "    io-publish tcp:1234 --size 24000 --on-demand --exec \"camera-cat arg1 arg2\"" << std::endl;
    std::cerr << "    io-publish tcp:1234 --size 24000 --on-demand -- camera-cat arg1 arg2" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( 0 );
}

class publish
{
    public:
        typedef comma::synchronized< boost::ptr_vector< comma::io::publisher > > publishers_t;
        
        typedef publishers_t::scoped_transaction transaction_t;
        
        publish( const std::vector< std::string >& filenames
               , unsigned int packet_size
               , bool discard
               , bool flush
               , bool output_number_of_clients
               , bool report_no_clients )
            : buffer_( packet_size, '\0' )
            , packet_size_( packet_size )
            , output_number_of_clients_( output_number_of_clients )
            , report_no_clients_( report_no_clients )
            , got_first_client_ever_( false )
            , sizes_( filenames.size(), 0 )
            , num_clients_( 0 )
            , is_shutdown_( false )
        {
            struct sigaction new_action, old_action;
            new_action.sa_handler = SIG_IGN;
            sigemptyset( &new_action.sa_mask );
            sigaction( SIGPIPE, NULL, &old_action );
            sigaction( SIGPIPE, &new_action, NULL );
            transaction_t t( publishers_ );
            for( std::size_t i = 0; i < filenames.size(); ++i )
            {
                t->push_back( new comma::io::publisher( filenames[i]
                                                      , is_binary_() ? comma::io::mode::binary : comma::io::mode::ascii
                                                      , !discard
                                                      , flush ));
            }
            acceptor_thread_.reset( new boost::thread( boost::bind( &publish::accept_, boost::ref( *this ))));
        }
        
        ~publish()
        {
            is_shutdown_ = true;
            acceptor_thread_->join();
            transaction_t t( publishers_ );
            { for( std::size_t i = 0; i < t->size(); ++i ) { ( *t )[i].close(); } }
        }
        
        bool read( std::istream& input )
        {
            if( is_binary_() )
            {
                input.read( &buffer_[0], buffer_.size() );
                if( input.gcount() < int( buffer_.size() ) || !input.good() ) { return false; }
            }
            else
            {
                std::getline( input, buffer_ );
                buffer_ += '\n';
                if( !input.good() ) { return false; }
            }
            transaction_t t( publishers_ );
            for( std::size_t i = 0; i < t->size(); ++i ) { ( *t )[i].write( &buffer_[0], buffer_.size(), false ); }
            return handle_sizes_( t );
        }

        unsigned int num_clients() const { return num_clients_; }

    private:
        bool is_binary_() const { return packet_size_ > 0; }
        
        bool handle_sizes_( transaction_t& t )
        {
            if( !output_number_of_clients_ && !report_no_clients_ ) { return true; }
            unsigned int total = 0;
            bool changed = false;
            for( unsigned int i = 0; i < t->size(); ++i )
            {
                unsigned int size = ( *t )[i].size();
                total += size;
                if( sizes_[i] == size ) { continue; }
                sizes_[i] = size;
                changed = true;
                num_clients_ = total;
            }
            if( !changed ) { return true; }
            if( output_number_of_clients_ )
            {
                std::cout << boost::posix_time::to_iso_string( boost::posix_time::microsec_clock::universal_time() );
                for( unsigned int i = 0; i < sizes_.size(); ++i ) { std::cout << ',' << sizes_[i]; }
                std::cout << std::endl;
            }
            if( report_no_clients_ )
            {
                if( total > 0 ) { got_first_client_ever_ = true; }
                else if( got_first_client_ever_ ) { std::cerr << "io-publish: the last client exited" << std::endl; return false; }
            }
            return true;
        }
        
        void accept_()
        {
            comma::io::select select;
            {
                transaction_t t( publishers_ );
                for( unsigned int i = 0; i < t->size(); ++i ) { if( ( *t )[i].acceptor_file_descriptor() != comma::io::invalid_file_descriptor ) { select.read().add( ( *t )[i].acceptor_file_descriptor() ); } }
            }
            while( !is_shutdown_ )
            {
                select.wait( boost::posix_time::millisec( 100 ) ); // arbitrary timeout
                transaction_t t( publishers_ );
                for( unsigned int i = 0; i < t->size(); ++i )
                {
                    if( select.read().ready( ( *t )[i].acceptor_file_descriptor() ) ) { ( *t )[i].accept(); }
                }
                handle_sizes_( t );
            }
        }
        
        publishers_t publishers_;
        std::string buffer_;
        unsigned int packet_size_;
        bool output_number_of_clients_;
        bool report_no_clients_;
        bool got_first_client_ever_;
        std::vector< unsigned int > sizes_;
        unsigned int num_clients_;
        boost::scoped_ptr< boost::thread > acceptor_thread_;
        bool is_shutdown_;
};

class command
{
    public:
        command( const std::string& cmd )
            : cmd_( cmd )
            , child_pid_( -1 )
        {
            comma::verbose << "launching " << cmd << std::endl;
            int fd[2];
            if( ::pipe( fd ) == -1 ) { comma::last_error::to_exception( "couldn't open pipe" ); } // create a pipe to send the child stdout to the parent stdin
            pid_t pid = fork();
            if( pid == -1 ) { comma::last_error::to_exception( "failed to fork()" ); }
            if( pid == 0 )
            {
                ::setsid(); // make the child a process group leader
                while( ( dup2( fd[1], STDOUT_FILENO ) == -1 ) && ( errno == EINTR ) ) {} // connect pipe input to stdout in child
                ::close( fd[1] );     // no longer need fd[1], now that it's duped
                ::close( fd[0] );     // don't need pipe output in the child
                ::execlp( "bash", "bash", "-c", &cmd_[0], NULL );
                std::cerr << "io-publish: failed to exec child: errno " << comma::last_error::value() << " - " << comma::last_error::to_string() << std::endl;
                exit( 1 );
            }
            child_pid_ = pid;
            while( ( ::dup2( fd[0], STDIN_FILENO ) == -1 ) && ( errno == EINTR ) ) {} // connect pipe output to stdin in parent
            ::close( fd[0] ); // no longer need fd[0], now that it's duped
            ::close( fd[1] ); // don't need pipe input in the parent
        }

        ~command()
        {
            comma::verbose << "killing child pid " << child_pid_ << " for " << cmd_ << "..." << std::endl;
            ::kill( -child_pid_, SIGTERM );
            comma::verbose << "waiting for pid " << child_pid_ << "..." << std::endl;
            if( ::waitpid( -child_pid_, NULL, 0 ) < 0 ) { comma::verbose << "warning: waiting for pid " << child_pid_ << " failed" << std::endl; }
            while( std::getchar() >= 0 ); // todo: lame, but select or c-style reading produce bizarre results; investigate further
            comma::verbose << "waiting for pid " << child_pid_ << " done" << std::endl;
        }

    private:
        std::string cmd_;
        pid_t child_pid_;
};

int main( int ac, char** av )
{
    try
    {
        //comma::command_line_options options( ac, av, usage );
        std::vector< std::string > head, tail;
        for( int i = 0; i < ac && std::string( "--" ) != av[i]; ++i ) { head.push_back( av[i] ); }
        for( int i = head.size() + 1; i < ac; ++i ) { tail.push_back( av[i] ); }
        comma::command_line_options options( head, usage );
        const std::vector< std::string >& names = options.unnamed( "--no-discard,--verbose,-v,--no-flush,--output-number-of-clients,--clients,--exit-on-no-clients,-e,--on-demand", "-.+" );
        if( names.empty() ) { std::cerr << "io-publish: please specify at least one stream; use '-' for stdout" << std::endl; return 1; }
        const boost::array< comma::signal_flag::signals, 2 > signals = { { comma::signal_flag::sigint, comma::signal_flag::sigterm } };
        comma::signal_flag is_shutdown( signals );
        bool on_demand = options.exists( "--on-demand" );
        bool exit_on_no_clients = options.exists( "--exit-on-no-clients,-e" );
        publish p( names
                 , options.value( "-s,--size", 0 ) * options.value( "-m,--multiplier", 1 )
                 , !options.exists( "--no-discard" )
                 , !options.exists( "--no-flush" )
                 , options.exists( "--output-number-of-clients,--clients" )
                 , exit_on_no_clients || on_demand );
        std::string exec_command = options.value< std::string >( "--exec", "" );
        if( !tail.empty() )
        {
            if( !exec_command.empty() ) { std::cerr << "io-publish: expected either --exec or --, got both" << std::endl; return 1; }
            exec_command = comma::join( tail, ' ' );
        }
        //ProfilerStart( "io-publish.prof" ); {
        if( exec_command.empty() )
        {
            while( std::cin.good() && !is_shutdown ) { if( !p.read( std::cin ) && exit_on_no_clients ) { break; } }
        }
        else
        {
            bool done = false;
            while( !done && !is_shutdown )
            {
                if( !on_demand || p.num_clients() > 0 )
                {
                    command cmd( exec_command );
                    while( std::cin.good() && !is_shutdown && p.read( std::cin ) );
                    if( !on_demand ) { done = true; }
                }
                else
                {
                    ::sleep( 0.1 );
                }
            }
        }
        //ProfilerStop(); }
        if( is_shutdown ) { std::cerr << "io-publish: interrupted by signal" << std::endl; }
        return 0;
    }
    catch( std::exception& ex )
    {
        if( comma::last_error::value() == EINTR || comma::last_error::value() == EBADF ) { return 0; }
        std::cerr << "io-publish: " << ex.what() << std::endl;
    }
    catch( ... )
    {
        if( comma::last_error::value() == EINTR || comma::last_error::value() == EBADF ) { return 0; }
        std::cerr << "io-publish: unknown exception" << std::endl;
    }
    return 1;
}

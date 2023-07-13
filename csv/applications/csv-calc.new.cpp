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


#include <string.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include "../../application/command_line_options.h"
#include "../../application/contact_info.h"
#include "../../base/types.h"
#include "../../csv/stream.h"
#include "../../csv/impl/unstructured.h"
#include "../../name_value/parser.h"
#include "../../string/string.h"
#include "../../visiting/traits.h"

static void usage()
{
    std::cerr << std::endl;
    std::cerr << "column-wise calculation, optionally by id and block" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat data.csv | csv-calc <what> [<options>] > calc.csv" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<what>: comma-separated list of operations" << std::endl;
    std::cerr << "        results will be output in the same order" << std::endl;
    std::cerr << "        optionally followed by block,id (both as ui, if binary)" << std::endl;
    std::cerr << "    min: minimum" << std::endl;
    std::cerr << "    max: maximum" << std::endl;
    std::cerr << "    mean: mean value" << std::endl;
    std::cerr << "    centre: ( min + max ) / 2" << std::endl;
    std::cerr << "    diameter: max - min" << std::endl;
    std::cerr << "    radius: size / 2" << std::endl;
    std::cerr << "    var: variance" << std::endl;
    std::cerr << "    stddev: standard deviation" << std::endl;
    std::cerr << "    size: number of values" << std::endl;
    std::cerr << "    sum: sum of values" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<options>" << std::endl;
    std::cerr << "    --delimiter,-d <delimiter> : default ','" << std::endl;
    std::cerr << "    --fields,-f: field names for which the calc should be computed, default: all fields" << std::endl;
    std::cerr << "                 if 'block' field present, calculate block-wise" << std::endl;
    std::cerr << "                 if 'id' field present, calculate by id" << std::endl;
    std::cerr << "                 if 'block' and 'id' fields present, calculate by id in each block" << std::endl;
    std::cerr << "                 block and id fields will be appended to the output" << std::endl;
    std::cerr << "    --format: in ascii mode: format hint string containing the types of the csv data, default: double or time" << std::endl;
    std::cerr << "    --binary,-b: in binary mode: format string of the csv data types" << std::endl;
    std::cerr << comma::csv::format::usage() << std::endl;
    std::cerr << std::endl;
    std::cerr << "examples" << std::endl;
    std::cerr << "    todo" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( 1 );
}

struct entry_t
{
    comma::csv::impl::unstructured values;
    comma::csv::impl::unstructured block;
    comma::csv::impl::unstructured id;
};

namespace comma { namespace visiting {

template <> struct traits< entry_t >
{
    template < typename K, typename V > static void visit( const K&, const entry_t& p, V& v )
    { 
        v.apply( "values", p.values );
        v.apply( "block", p.block );
        v.apply( "id", p.id );
    }

    template < typename K, typename V > static void visit( const K&, entry_t& p, V& v )
    { 
        v.apply( "values", p.values );
        v.apply( "block", p.block );
        v.apply( "id", p.id );
    }
};

} } // namespace comma { namespace visiting {

struct operation
{
    virtual ~operation() {}
    virtual void reset() {}
    virtual void update( const comma::csv::impl::unstructured& v ) = 0;
    virtual void finalize( comma::csv::impl::unstructured& r ) {}
};

template < typename Derived >
class tied_operation : public operation
{
    public:
        tied_operation() : i_( 0 ), j_( 0 ) {}
        tied_operation( std::size_t i, comma::csv::impl::unstructured& r )
            : i_( i )
            , j_( r.get< typename Derived::result_type >().size() )
        {
            r.get< typename Derived::result_type >().push_back( typename Derived::result_type() );
        }
        
    protected:
        std::size_t i_;
        std::size_t j_;
};

static std::vector< std::string > reset_all_( const std::string& field, const std::vector< std::string >& fields )
{
    std::vector< std::string > f = fields;
    for( unsigned int i = 0; i < f.size(); ++i ) { if( f[i] == field ) { f[i].clear(); } }
    return f;
}

static std::vector< std::string > reset_all_but_( const std::string& field, const std::vector< std::string >& fields )
{
    std::vector< std::string > f = fields;
    for( unsigned int i = 0; i < f.size(); ++i ) { if( f[i] != field ) { f[i].clear(); } }
    return f;
}

static std::pair< entry_t, comma::csv::options > make_input_( const comma::csv::options& csv, const std::string& sample = "" )
{
    std::vector< std::string > f = comma::split( csv.fields, "," );
    comma::csv::options values_csv = csv;
    values_csv.fields = comma::join( reset_all_( "id", reset_all_( "block", f ) ), ',' );
    std::pair< comma::csv::impl::unstructured, comma::csv::options > v = comma::csv::impl::unstructured::make( values_csv, sample );
    comma::csv::options block_csv = csv;
    block_csv.fields = comma::join( reset_all_but_( "block", f ), ',' );
    std::pair< comma::csv::impl::unstructured, comma::csv::options > b = comma::csv::impl::unstructured::make( block_csv, sample );
    comma::csv::options id_csv = csv;
    id_csv.fields = comma::join( reset_all_but_( "id", f ), ',' );
    std::pair< comma::csv::impl::unstructured, comma::csv::options > i = comma::csv::impl::unstructured::make( id_csv, sample );
    std::pair< entry_t, comma::csv::options > p;
    p.first.values = v.first;
    p.first.block = b.first;
    p.first.id = i.first;
    std::vector< std::string > vv = comma::split( v.second.fields, ',' );
    std::vector< std::string > vb = comma::split( b.second.fields, ',' );
    std::vector< std::string > vi = comma::split( i.second.fields, ',' );
    std::vector< std::string > vf;
    for( std::size_t i = 0; i < std::max( vv.size(), std::max( vb.size(), vi.size() ) ); ++i )
    {
        if( i < vv.size() && !vv[i].empty() ) { vf.push_back( "values/" + vv[i] ); }
        else if( i < vb.size() && !vb[i].empty() ) { vf.push_back( "block/" + vb[i] ); }
        else if( i < vi.size() && !vi[i].empty() ) { vf.push_back( "id/" + vi[i] ); }
        else { vf.push_back( "" ); }
    }
    p.second.fields = comma::join( vf, ',' );
    p.second.full_xpath = true;
    return p;
}

// static std::pair< entry_t, comma::csv::options > make_output_( const std::vector< std::string >& operations, const comma::csv::options& csv, const std::string& sample = "" )
// {
//     // todo
//     return std::pair< entry_t, comma::csv::options >();
// }

template < typename T > struct bound_result_traits
{ 
    static std::string name();
    static std::string format();
};

template <> struct bound_result_traits< double >
{ 
    static std::string name() { return "d"; }
    static std::string format() { return "d"; }
};

template <> struct bound_result_traits< std::string >
{ 
    static std::string name() { return "s"; }
    static std::string format() { return "s[256]"; }
};

template <> struct bound_result_traits< boost::posix_time::ptime >
{ 
    static std::string name() { return "t"; }
    static std::string format() { return "t"; }
};


// class bound_result
// {
//     public:
//         template < typename D, typename S, typename T >
//         bound_result( const comma::csv::impl::unstructured& sample
//                     , const operation< double, D >& dop
//                     , const operation< std::string, S >& sop
//                     , const operation< boost::posix_time::ptime, T >& top
//                     , comma::csv::impl::unstructured& result )
//             : result_( result )
//         {
//             
//         }
//         
//     private:
//         comma::csv::impl::unstructured& result_;
// };
// 
// 
// class bound_result // real quick and dirty
// {
//     public:
//         template < typename T, typename R >
//         struct functors
//         {
//             boost::function< void( const T& ) > update;
//             boost::function< R() > result;
//             
//             functors() {}
//             functors( boost::function< void( R& ) > reset
//                     , boost::function< void( R&, const T& ) > update
//                     , boost::function< void( R& ) > finalize )
//                 : reset( reset )
//                 , update( update )
//                 , finalize( finalize )
//             {
//             }
//         };
//         
//         template < typename T >
//         struct bound_functors
//         {
//             boost::function< void() > reset;
//             boost::function< void( const T& ) > update;
//             boost::function< void() > finalize;
//             
//             bound_functors() {}
//             bound_functors( boost::function< void() > reset
//                           , boost::function< void( const T& ) > update
//                           , boost::function< void() > finalize )
//                 : reset( reset )
//                 , update( update )
//                 , finalize( finalize )
//             {
//             }
//         };
//         
//         bound_result() : result_( NULL ) {}
//         
//         template < typename D, typename S, typename T >
//         bound_result( comma::csv::impl::unstructured& result
//                     , functors< D, double > df
//                     , functors< S, std::string > sf
//                     , functors< T, boost::posix_time::ptime > tf
//                     , const comma::csv::impl::unstructured& sample )
//             : result_( &result )
//         {
//             init_( df, sample.doubles.size() );
//             init_( sf, sample.strings.size() );
//             init_( tf, sample.timestamps.size() );
//         }
//         
//         template < typename Traits >
//         static comma::csv::options options( const comma::csv::options input_csv ) // quick and dirty
//         {
//             comma::csv::options csv;
//             std::vector< std::string > f = comma::split( input_csv.fields, ',' );
//             std::vector< std::string > fields;
//             std::vector< std::string > format;
//             std::string comma;
//             std::size_t double_fields_count = 0;
//             std::size_t string_fields_count = 0;
//             std::size_t time_fields_count = 0;
//             for( std::size_t i = 0; i < f.size(); ++i )
//             {
//                 if( f[i].empty() ) { continue; }
//                 std::vector< std::string > v = comma::split( f[i], '/' );
//                 if( v[0] == "block" || v[0] == "id" ) { continue; } // way quick and dirty
//                 
//                 switch( csv.format().offset( i ).type )
//                 {
//                     case comma::csv::format::time:
//                     case comma::csv::format::long_time:
//                         fields.push_back( bound_result_traits< typename Traits::template result_t< boost::posix_time::ptime > >::name() + "[" + boost::lexical_cast< std::string >( time_fields_count++ ) + "]" );
//                         format.push_back( bound_result_traits< typename Traits::template result_t< boost::posix_time::ptime > >::format() );
//                         break;
//                     case comma::csv::format::fixed_string:
//                         fields.push_back( bound_result_traits< typename Traits::template result_t< std::string > >::name() + "[" + boost::lexical_cast< std::string >( string_fields_count++ ) + "]" );
//                         format.push_back( bound_result_traits< typename Traits::template result_t< std::string > >::format() );
//                         break;
//                     default:
//                         fields.push_back( bound_result_traits< typename Traits::template result_t< double > >::name() + "[" + boost::lexical_cast< std::string >( double_fields_count++ ) + "]" );
//                         format.push_back( bound_result_traits< typename Traits::template result_t< double > >::format() );
//                         break;
//                 }
//             }
//             csv.fields = comma::join( fields, ',' );
//             csv.format( comma::join( format, ',' ) );
//             return csv;
//         }
//         
//         void reset()
//         {
//             for( std::size_t i = 0; i < df_.size(); ++i ) { df_[i].reset(); }
//             for( std::size_t i = 0; i < sf_.size(); ++i ) { sf_[i].reset(); }
//             for( std::size_t i = 0; i < tf_.size(); ++i ) { tf_[i].reset(); }
//         }
//         
//         void update( const comma::csv::impl::unstructured& v )
//         {
//             for( std::size_t i = 0; i < v.doubles.size(); ++i ) { df_[i].update( v.doubles[i] ); }
//             for( std::size_t i = 0; i < v.strings.size(); ++i ) { sf_[i].update( v.strings[i] ); }
//             for( std::size_t i = 0; i < v.timestamps.size(); ++i ) { tf_[i].update( v.timestamps[i] ); }
//         }
//         
//         const comma::csv::impl::unstructured& finalize()
//         {
//             for( std::size_t i = 0; i < df_.size(); ++i ) { df_[i].finalize(); }
//             for( std::size_t i = 0; i < sf_.size(); ++i ) { sf_[i].finalize(); }
//             for( std::size_t i = 0; i < tf_.size(); ++i ) { tf_[i].finalize(); }
//             return *result_;
//         }
//         
//         const comma::csv::impl::unstructured& result() const { return *result_; };
//         
//     private:
//         template < typename T, typename R >
//         void init_( const functors< T, R >& f, std::size_t size )
//         {
//             for( std::size_t i = 0; i < size; ++i )
//             {
//                 get_< R >( *result_ ).push_back( R() );
//                 bound_functors< T > b( boost::bind( f.reset, boost::ref( get_< R >( *result_ ).back() ) )
//                                      , boost::bind( f.update, boost::ref( get_< R >( *result_ ).back(), boost::placeholders::_1 ) )
//                                      , boost::bind( f.finalize, boost::ref( get_< R >( *result_ ).back() ) ) );
//                 get_< T >().push_back( b );
//             }
//         }
//         
//         template < typename T > std::vector< bound_functors< T > >& get_();
//         
//         std::vector< bound_functors< double > > df_;
//         std::vector< bound_functors< std::string > > sf_;
//         std::vector< bound_functors< boost::posix_time::ptime > > tf_;
//         comma::csv::impl::unstructured* result_;
// };
// 
// template <> std::vector< bound_result::bound_functors< double > >& bound_result::get_< double >() { return df_; }
// template <> std::vector< bound_result::bound_functors< std::string > >& bound_result::get_< std::string >() { return sf_; }
// template <> std::vector< bound_result::bound_functors< boost::posix_time::ptime > >& bound_result::get_< boost::posix_time::ptime >() { return tf_; }

// namespace operations {
// 
// template < typename T > struct min
// {
//     typedef T type;
//     typedef T result_type;
//     static void apply( result_type& lhs, const T& rhs ) { if( rhs < lhs ) { lhs = rhs; } }
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &min::apply, boost::placeholders::_1, _2 ), std::numeric_limits< T >::max() ); }
// };
// 
// template <> struct min< std::string >
// {
//     typedef std::string type;
//     typedef std::string result_type;
//     static void apply( result_type&, const std::string& ) {}
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &min::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// template <> struct min< boost::posix_time::ptime >
// {
//     typedef boost::posix_time::ptime type;
//     typedef boost::posix_time::ptime result_type;
//     static void apply( boost::posix_time::ptime& lhs, const boost::posix_time::ptime& rhs ) { if( lhs.is_not_a_date_time() || rhs < lhs ) { lhs = rhs; } }
//     static operation::options< result_type, boost::posix_time::ptime > options() { return operation::options< result_type, boost::posix_time::ptime >( boost::bind( &min::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// template < typename T > struct max
// {
//     typedef T type;
//     typedef T result_type;
//     static void apply( result_type& lhs, const T& rhs ) { if( lhs < rhs ) { lhs = rhs; } }
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &max::apply, boost::placeholders::_1, _2 ), std::numeric_limits< T >::min() ); }
// };
// 
// template <> struct max< std::string >
// {
//     typedef std::string type;
//     typedef std::string result_type;
//     static void apply( result_type& lhs, const std::string& rhs ) { if( lhs < rhs ) { lhs = rhs; } }
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &max::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// template <> struct min< boost::posix_time::ptime >
// {
//     typedef boost::posix_time::ptime type;
//     typedef boost::posix_time::ptime result_type;
//     static void apply( boost::posix_time::ptime& lhs, const boost::posix_time::ptime& rhs ) { if( lhs.is_not_a_date_time() || rhs < lhs ) { lhs = rhs; } }
//     static operation::options< result_type, boost::posix_time::ptime > options() { return operation::options< result_type, boost::posix_time::ptime >( boost::bind( &max::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// template < typename T > struct size
// {
//     typedef T type;
//     typedef double result_type;
//     static void apply( result_type& lhs, const T& rhs ) { ++rhs; }
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &size::apply, boost::placeholders::_1, _2 ), 0 ); }
// };
// 
// template < typename T > struct sum
// {
//     typedef T type;
//     typedef T result_type;
//     static void apply( result_type& lhs, const T& rhs ) { lhs += rhs; }
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &sum::apply, boost::placeholders::_1, _2 ), 0 ); }
// };
// 
// template <> struct sum< std::string >
// {
//     typedef std::string type;
//     typedef std::string result_type;
//     static void apply( result_type&, const std::string& ) {}
//     static operation::options< result_type, T > options() { return operation::options< result_type, T >( boost::bind( &sum::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// template <> struct sum< boost::posix_time::ptime >
// {
//     typedef boost::posix_time::ptime type;
//     typedef boost::posix_time::ptime result_type;
//     static void apply( boost::posix_time::ptime&, const boost::posix_time::ptime& ) {}
//     static operation::options< result_type, boost::posix_time::ptime > options() { return operation::options< result_type, boost::posix_time::ptime >( boost::bind( &sum::apply, boost::placeholders::_1, _2 ) ); }
// };
// 
// } // namespace operations {

static bool verbose;
static comma::csv::impl::unstructured block;
static entry_t input;
static entry_t output;
static comma::csv::options stdin_csv;
static comma::csv::options stdout_csv;
static boost::scoped_ptr< comma::csv::input_stream< entry_t > > istream;
static boost::scoped_ptr< comma::csv::output_stream< entry_t > > ostream;

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av );
        if( options.exists( "--help,-h,--long-help" ) ) { usage(); }
        verbose = options.exists( "--verbose,-v" );
        stdin_csv = comma::csv::options( options );
        if( stdin_csv.binary() )
        {
            std::pair< entry_t, comma::csv::options > p = make_input_( stdin_csv );
            input = p.first;
            stdin_csv = p.second;
            istream.reset( new comma::csv::input_stream< entry_t >( std::cin, stdin_csv, input ) );
            if( verbose ) { std::cerr << "csv-calc: input fields: " << stdin_csv.fields << std::endl; } // todo: remove
        }
        std::vector< std::string > unnamed = options.unnamed( "--verbose,-v", "--binary,-b,--delimiter,-d,--fields,-f" );
        if( unnamed.empty() ) { std::cerr << "csv-calc: please specify operations" << std::endl; return 1; }
        std::vector< std::string > operation_strings = comma::split( unnamed[0], ',' );
        
        
        
        return 0;
    }
    catch( std::exception& ex )
    {
        std::cerr << "csv-calc: " << ex.what() << std::endl;
    }
    catch( ... )
    {
        std::cerr << "csv-calc: unknown exception" << std::endl;
    }
    return 1;
}

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

#ifdef WIN32
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#endif

#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "../../application/contact_info.h"
#include "../../application/verbose.h"
#include "../../base/exception.h"
#include "../../csv/format.h"
#include "../../csv/options.h"
#include "../../string/string.h"

static void bash_completion( unsigned const ac, char const * const * av )
{
    static char const * const arguments =
        " min max mean mode percentile sum centre diameter radius var stddev size"
        " --append"
        " --delimiter -d"
        " --fields -f"
        " --output-fields"
        " --output-format"
        " --format"
        " --binary -b"
        " --verbose -v";
    std::cout << arguments << std::endl;
    exit( 0 );
}

static void usage( bool verbose )
{
    std::cerr << std::endl;
    std::cerr << "column-wise calculation, optionally by id and block" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat data.csv | " << comma::verbose.app_name() << " <what> [<options>] > calc.csv" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<what>: comma-separated list of operations" << std::endl;
    std::cerr << "        results will be output in the same order" << std::endl;
    std::cerr << "        optionally followed by block,id (both as ui, if binary)" << std::endl;
    std::cerr << "    centre: ( min + max ) / 2" << std::endl;
    std::cerr << "    diameter: max - min" << std::endl;
    std::cerr << "    kurtosis[=sample|excess]: kurtosis" << std::endl;
    std::cerr << "         sample: use sample kurtosis (default: population kurtosis)" << std::endl;
    std::cerr << "         excess: calculate excess kurtosis" << std::endl;
    std::cerr << "    max: maximum" << std::endl;
    std::cerr << "    mean: mean value" << std::endl;
    std::cerr << "    min: minimum" << std::endl;
    std::cerr << "    mode: mode value" << std::endl;
    std::cerr << "    percentile=<n>[:<method>]: percentile value" << std::endl;
    std::cerr << "        <n> is the desired percentile (e.g. 0.9)" << std::endl;
    std::cerr << "        <method> is one of 'nearest' or 'interpolate' (default: nearest)" << std::endl;
    std::cerr << "        see --help --verbose for more details" << std::endl;
    std::cerr << "    radius: size / 2" << std::endl;
    std::cerr << "    size: number of values" << std::endl;
    std::cerr << "    skew[=sample]: skew" << std::endl;
    std::cerr << "         sample: use sample skew (default: population stddev)" << std::endl;
    std::cerr << "    stddev[=sample]: standard deviation" << std::endl;
    std::cerr << "         sample: use sample stddev (default: population stddev)" << std::endl;
    std::cerr << "    sum: sum" << std::endl;
    std::cerr << "    var[=sample]: variance" << std::endl;
    std::cerr << "         sample: use sample variance (default: population variance)" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<options>" << std::endl;
    std::cerr << "    --append: append statistics to each input line" << std::endl;
    std::cerr << "    --delimiter,-d <delimiter> : default ','" << std::endl;
    std::cerr << "    --fields,-f: field names for which the extents should be computed, default: all fields" << std::endl;
    std::cerr << "                 if 'block' field present, calculate block-wise" << std::endl;
    std::cerr << "                 if 'id' field present, calculate by id" << std::endl;
    std::cerr << "                 if 'block' and 'id' fields present, calculate by id in each block" << std::endl;
    std::cerr << "                 block and id fields will be appended to the output" << std::endl;
    std::cerr << "    --output-fields: print output field names for these operations and then exit" << std::endl;
    std::cerr << "    --output-format: print output format for this operation and then exit (note: requires input-format)" << std::endl;
    std::cerr << "    --format: in ascii mode: format hint string containing the types of the csv data, default: double or time" << std::endl;
    std::cerr << "    --binary,-b: in binary mode: format string of the csv data types" << std::endl;
    std::cerr << "    --verbose,-v: more output to stderr" << std::endl;
    std::cerr << comma::csv::format::usage() << std::endl;
    if( verbose )
    {
        std::cerr << "percentile method:" << std::endl;
        std::cerr << "    The percentile method is either 'nearest' or 'interpolate'." << std::endl;
        std::cerr << "    For an overview of percentile calculation methods see" << std::endl;
        std::cerr << "    https://en.wikipedia.org/wiki/Percentile." << std::endl;
        std::cerr << std::endl;
        std::cerr << "    'nearest' gives the smallest value in the list such that the requested" << std::endl;
        std::cerr << "    fraction (percentile) of the data is less than or equal to that value." << std::endl;
        std::cerr << std::endl;
        std::cerr << "    'interpolate' takes the two nearest values from the list and interpolates" << std::endl;
        std::cerr << "    between them according to the NIST recommended linear interpolation method." << std::endl;
        std::cerr << "    See http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm." << std::endl;
        std::cerr << std::endl;
        std::cerr << "    For a really detailed analysis see the nine methods discussed in" << std::endl;
        std::cerr << "    Hyndman, R.J. and Fan, Y. (November 1996)." << std::endl;
        std::cerr << "    \"Sample Quantiles in Statistical Packages\"," << std::endl;
        std::cerr << "    The American Statistician 50 (4): pp. 361-365." << std::endl;
        std::cerr << "    'nearest' corresponds to definition 1." << std::endl;
        std::cerr << "    'interpolate' corresponds to definition 6." << std::endl;
        std::cerr << std::endl;
    }
    std::cerr << "examples" << std::endl;
    std::cerr << "    seq 1 1000 | " << comma::verbose.app_name() << " percentile=0.9" << std::endl;
    std::cerr << "    seq 1 1000 | " << comma::verbose.app_name() << " percentile=0.1,percentile=0.9" << std::endl;
    std::cerr << "    seq 1 1000 | " << comma::verbose.app_name() << " percentile=0.9:interpolate --verbose" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    {(seq 1 500 | csv-paste \"-\" \"value=0\") ; (seq 1 100 | csv-paste \"-\" \"value=1\") ; (seq 501 1000 | csv-paste \"-\" \"value=0\")} | " << comma::verbose.app_name() << " --fields=a,block percentile=0.9" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    {(seq 1 500 | csv-paste \"-\" \"value=0\") ; (seq 1 100 | csv-paste \"-\" \"value=1\") ; (seq 501 1000 | csv-paste \"-\" \"value=0\")} | " << comma::verbose.app_name() << " --fields=a,id percentile=0.9" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( -1 );
}

class Values
{
    public:
        Values( const comma::csv::options& csv, const comma::csv::format& input_format )
            : csv_( csv )
            , input_format_( input_format )
            , block_( 0 )
            , id_( 0 )
        {
            init_indices_();
            init_format_();
        }

        Values( const comma::csv::options& csv, const std::string& hint )
            : csv_( csv )
            , block_( 0 )
            , id_( 0 )
        {
            init_indices_();
            std::vector< std::string > v = comma::split( hint, csv.delimiter );
            for( unsigned int i = 0; i < v.size(); ++i )
            {
                if( ( block_index_ && *block_index_ == i ) || ( id_index_ && *id_index_ == i ) ) { input_format_ += "ui"; continue; }
                try { boost::posix_time::from_iso_string( v[i] ); input_format_ += "t"; }
                catch( ... ) { input_format_ += "d"; }
            }
            std::cerr << comma::verbose.app_name() << ": guessed format: " << input_format_.string() << std::endl;
            init_format_();
        }

        void set( const char* buf )
        {
            for( unsigned int i = 0; i < indices_.size(); ++i )
            {
                ::memcpy( &buffer_[0] + elements_[i].offset, buf + input_elements_[i].offset, elements_[i].size );
            }
            if( block_index_ ) { block_ = block_from_bin_( buf + block_element_.offset ); }
            if( id_index_ ) { id_ = id_from_bin_( buf + id_element_.offset ); }
        }

        void set( const std::string& line ) // quick and dirty, probably very slow
        {
            std::vector< std::string > v = comma::split( line, csv_.delimiter );
            std::vector< std::string > w( indices_.size() );
            for( unsigned int i = 0; i < indices_.size(); ++i ) { w[i] = v[indices_[i]]; }
            const std::string& s = format_.csv_to_bin( w );
            ::memcpy( &buffer_[0], &s[0], buffer_.size() );
            if( block_index_ ) { block_ = boost::lexical_cast< unsigned int >( v[ *block_index_ ] ); }
            if( id_index_ ) { id_ = boost::lexical_cast< unsigned int >( v[ *id_index_ ] ); }
        }

        const comma::csv::format& format() const { return format_; }
        unsigned int block() const { return block_; }
        unsigned int id() const { return id_; }
        const char* buffer() const { return &buffer_[0]; }

    private:
        comma::csv::options csv_;
        comma::csv::format input_format_;
        comma::csv::format format_;
        std::vector< unsigned int > indices_;
        std::vector< comma::csv::format::element > input_elements_;
        std::vector< comma::csv::format::element > elements_;
        std::vector< char > buffer_;
        boost::optional< unsigned int > block_index_;
        boost::optional< unsigned int > id_index_;
        comma::csv::format::element block_element_;
        comma::csv::format::element id_element_;
        unsigned int block_;
        unsigned int id_;
        boost::function< comma::uint32( const char* ) > block_from_bin_;
        boost::function< comma::uint32( const char* ) > id_from_bin_;
        template < typename T > static comma::uint32 from_bin_( const char* buf ) { return comma::csv::format::traits< T >::from_bin( buf ); }

        void init_indices_()
        {
            std::vector< std::string > v = comma::split( csv_.fields, ',' );
            for( unsigned int i = 0; i < v.size(); ++i )
            {
                if( v[i] == "block" ) { block_index_ = i; }
                else if( v[i] == "id" ) { id_index_ = i; }
                else if( v[i] != "" ) { indices_.push_back( i ); }
            }
        }
        void init_format_()
        {
            if( indices_.empty() )
            {
                for( unsigned int i = 0; i < input_format_.count(); ++i )
                {
                    if( block_index_ && *block_index_ == i ) { continue; }
                    if( id_index_ && *id_index_ == i ) { continue; }
                    indices_.push_back( i );
                }
            }
            for( unsigned int i = 0; i < indices_.size(); ++i )
            {
                comma::csv::format::element e = input_format_.offset( indices_[i] );
                format_ += comma::csv::format::to_format( e.type, e.size );
            }
            for( unsigned int i = 0; i < indices_.size(); ++i )
            {
                elements_.push_back( format_.offset( i ) );
                input_elements_.push_back( input_format_.offset( indices_[i] ) );
            }
            buffer_.resize( format_.size() );
            if( block_index_ )
            {
                block_element_ = input_format_.offset( *block_index_ );
                switch( block_element_.type )
                {
                    case comma::csv::format::char_t: block_from_bin_ = boost::bind( &Values::from_bin_< char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int8: block_from_bin_ = boost::bind( &Values::from_bin_< char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint8: block_from_bin_ = boost::bind( &Values::from_bin_< unsigned char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int16: block_from_bin_ = boost::bind( &Values::from_bin_< comma::int16 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint16: block_from_bin_ = boost::bind( &Values::from_bin_< comma::uint16 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int32: block_from_bin_ = boost::bind( &Values::from_bin_< comma::int32 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint32: block_from_bin_ = boost::bind( &Values::from_bin_< comma::uint32 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int64: block_from_bin_ = boost::bind( &Values::from_bin_< comma::int64 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint64: block_from_bin_ = boost::bind( &Values::from_bin_< comma::uint64 >, boost::placeholders::_1 ); break;
                    default: COMMA_THROW( comma::exception, "expected integer for block id, got format " << input_format_.string() );
                }
            }
            if( id_index_ )
            {
                id_element_ = input_format_.offset( *id_index_ );
                switch( id_element_.type )
                {
                    case comma::csv::format::char_t: id_from_bin_ = boost::bind( &Values::from_bin_< char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int8: id_from_bin_ = boost::bind( &Values::from_bin_< char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint8: id_from_bin_ = boost::bind( &Values::from_bin_< unsigned char >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int16: id_from_bin_ = boost::bind( &Values::from_bin_< comma::int16 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint16: id_from_bin_ = boost::bind( &Values::from_bin_< comma::uint16 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int32: id_from_bin_ = boost::bind( &Values::from_bin_< comma::int32 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint32: id_from_bin_ = boost::bind( &Values::from_bin_< comma::uint32 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::int64: id_from_bin_ = boost::bind( &Values::from_bin_< comma::int64 >, boost::placeholders::_1 ); break;
                    case comma::csv::format::uint64: id_from_bin_ = boost::bind( &Values::from_bin_< comma::uint64 >, boost::placeholders::_1 ); break;
                    default: COMMA_THROW( comma::exception, "expected integer for block id, got format " << input_format_.string() );
                }
            }
        }
};

class ascii_input
{
    public:
        ascii_input( const comma::csv::options& csv, const boost::optional< comma::csv::format >& format ) : csv_( csv )
        {
            if( format ) { values_.reset( new Values( csv, *format ) ); }
        }

        const Values* read()
        {
            std::getline( std::cin, line_ );
            if( line_ == "" ) { return NULL; }
            if( !values_ ) { values_.reset( new Values( csv_, line_ ) ); }
            values_->set( line_ );
            return values_.get();
        }

        const std::string line() { return line_; }
        
    private:
        comma::csv::options csv_;
        boost::scoped_ptr< Values > values_;
        std::string line_;
};

class binary_input
{
    public:
        binary_input( const comma::csv::options& csv )
            : csv_( csv )
            , values_( csv, csv.format() )
            , buffer_( csv.format().size() > 65536 ? csv.format().size() : 65536 / csv.format().size() * csv.format().size() )
            , cur_( &buffer_[0] )
            , end_( &buffer_[0] + buffer_.size() )
            , offset_( 0 )
        {
        }

        const Values* read()
        {
            while( true )
            {
                if( offset_ >= csv_.format().size() )
                {
                    values_.set( cur_ );
                    line_ = std::string(cur_, csv_.format().size());
                    cur_ += csv_.format().size();
                    offset_ -= csv_.format().size();
                    if( cur_ == end_ ) { cur_ = &buffer_[0]; offset_ = 0; }
                    return &values_;
                }
                int count = ::read( 0, cur_ + offset_, end_ - cur_ - offset_ );
                if( count <= 0 ) { return NULL; }
                offset_ += count;
            }
        }
        const std::string line() { return line_; }

    private:
        comma::csv::options csv_;
        Values values_;
        std::vector< char > buffer_;
        char* cur_;
        const char* end_;
        unsigned int offset_;
        std::string line_;
};

namespace impl {

template < typename T, typename V > struct map_traits
{
    typedef boost::unordered_map< T, V > unordered_map;
};

template < typename V > struct map_traits< boost::posix_time::ptime, V >
{
    struct hash : public std::unary_function< boost::posix_time::ptime, std::size_t >
    {
        std::size_t operator()( const boost::posix_time::ptime& t ) const
        {
            BOOST_STATIC_ASSERT( sizeof( t ) == sizeof( comma::uint64 ) );
            std::size_t seed = 0;
            boost::hash_combine( seed, reinterpret_cast< const comma::uint64& >( t ) ); // quick and dirty
            return seed;
        }
    };

    typedef boost::unordered_map< boost::posix_time::ptime, V, hash > unordered_map;
};

// todo: reasonable hash for float and double
// template < typename V > struct map_traits< float, V >
// {
//     struct hash : public std::unary_function< float, std::size_t >
//     {
//         std::size_t operator()( const float& t ) const
//         {
//             std::size_t seed = 0;
//             boost::hash_combine( seed, t ); // quick and dirty
//             return seed;
//         }
//     };
// 
//     typedef boost::unordered_map< float, V, hash > unordered_map;
// };
    
template < typename T >
class value_count
{
    public:
        typedef typename map_traits< T, comma::uint32 >::unordered_map map_t;
        
        void update( const T& t )
        {
            typename map_t::iterator it = map_.find( t );
            if( it == map_.end() ) { map_[t] = 1; } else { ++( it->second ); }
        }
        
        const map_t& map() const { return map_; }
        
        typename map_t::value_type mode() const
        {
            typename map_t::const_iterator best = map_.begin();
            for( typename map_t::const_iterator it = map_.begin(); it != map_.end(); ++it ) { if( it->second > best->second ) { best = it; } }
            return *best;
        }
    
    private:
        map_t map_;
};

} // namespace impl {

namespace Operations
{
    struct base
    {
        virtual ~base() {}
        virtual void push( const char* ) = 0;
        virtual void calculate( char* ) = 0;
        virtual base* clone() const = 0;
        virtual void set_options( const std::vector< std::string >& options ) {}
    };

    template < typename T, comma::csv::format::types_enum F > class Centre;
    template < typename T, comma::csv::format::types_enum F > class Radius;
    template < typename T, comma::csv::format::types_enum F > class Diameter;

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Min : public base
    {
        public:
            void push( const char* buf )
            {
                const T& t = comma::csv::format::traits< T, F >::from_bin( buf );
                if( !min_ || t < *min_ ) { min_ = t; }
            }
            void calculate( char* buf ) { if( min_ ) { comma::csv::format::traits< T, F >::to_bin( *min_, buf ); } }
            base* clone() const { return new Min< T, F >( *this ); }
        private:
            friend class Centre< T, F >;
            friend class Diameter< T, F >;
            friend class Radius< T, F >;
            boost::optional< T > min_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Max : public base
    {
        public:
            void push( const char* buf )
            {
                T t = comma::csv::format::traits< T, F >::from_bin( buf );
                if( !max_ || t > *max_ ) { max_ = t; }
            }
            void calculate( char* buf ) { if( max_ ) { comma::csv::format::traits< T, F >::to_bin( *max_, buf ); } }
            base* clone() const { return new Max< T, F >( *this ); }
        private:
            friend class Centre< T, F >;
            friend class Diameter< T, F >;
            friend class Radius< T, F >;
            boost::optional< T > max_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Sum : public base
    {
        public:
            void push( const char* buf )
            {
                T t = comma::csv::format::traits< T, F >::from_bin( buf );
                sum_ = sum_ ? *sum_ + t : t;
            }
            void calculate( char* buf ) { if( sum_ ) { comma::csv::format::traits< T, F >::to_bin( *sum_, buf ); } }
            base* clone() const { return new Sum< T, F >( *this ); }
        private:
            boost::optional< T > sum_;
    };

    template < comma::csv::format::types_enum F >
    class Sum< boost::posix_time::ptime, F > : public base
    {
        void push( const char* ) { COMMA_THROW( comma::exception, "sum not defined for time" ); }
        void calculate( char* ) { COMMA_THROW( comma::exception, "sum not defined for time" ); }
        base* clone() const { COMMA_THROW( comma::exception, "sum not defined for time" ); }
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Centre : public base
    {
        public:
            void push( const char* buf ) { min_.push( buf ); max_.push( buf ); }
            void calculate( char* buf ) { if( min_.min_ ) { comma::csv::format::traits< T, F >::to_bin( *min_.min_ + ( *max_.max_ - *min_.min_ ) / 2, buf ); } }
            base* clone() const { return new Centre< T, F >( *this ); }
        private:
            Min< T, F > min_;
            Max< T, F > max_;
    };

    template < typename T > struct result_traits { typedef double type; }; // quick and dirty fix for integer T
    template <> struct result_traits< boost::posix_time::ptime > { typedef boost::posix_time::ptime type; }; // quick and dirty fix for integer T
    
    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Mode : public base
    {
        public:
            void push( const char* buf ) { value_count_.update( comma::csv::format::traits< T, F >::from_bin( buf ) ); }
            void calculate( char* buf ) { if( !value_count_.map().empty() ) { comma::csv::format::traits< T, F >::to_bin( static_cast< T >( value_count_.mode().first ), buf ); } }
            base* clone() const { return new Mode< T, F >( *this ); }
        private:
            impl::value_count< T > value_count_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Mean : public base
    {
        public:
            Mean() : count_( 0 ) {}
            void push( const char* buf )
            {
                T t = comma::csv::format::traits< T, F >::from_bin( buf );
                ++count_;
                mean_ = mean_ ? *mean_ + ( t - *mean_ ) / count_ : t ;
            }
            void calculate( char* buf ) { if( count_ > 0 ) { comma::csv::format::traits< T, F >::to_bin( static_cast< T >( *mean_ ), buf ); } }
            base* clone() const { return new Mean< T, F >( *this ); }
        private:
            boost::optional< typename result_traits< T >::type > mean_;
            std::size_t count_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Percentile : public base
    {
        public:
            enum Method { nearest, interpolate };

            Percentile() : percentile_( 0.0 ), method_( nearest ) {}

            void push( const char* buf )
            {
                values_.insert( comma::csv::format::traits< T, F >::from_bin( buf ));
            }

            void set_options( const std::vector< std::string >& options )
            {
                if( options.size() == 0 ) {
                    std::cerr << comma::verbose.app_name() << ": percentile operation requires a percentile" << std::endl;
                    exit( 1 );
                }

                percentile_ = boost::lexical_cast< double >( options[0] );
                if( percentile_ < 0.0 || percentile_ > 1.0 ) {
                    std::cerr << comma::verbose.app_name() << ": percentile value should be between 0 and 1, got " << percentile_ << std::endl;
                    exit( 1 );
                }

                if( options.size() == 2 ) {
                    if( options[1] == "nearest" ) method_ = nearest;
                    else if( options[1] == "interpolate" ) method_ = interpolate;
                    else {
                        std::cerr << comma::verbose.app_name() << ": expected percentile method, got " << options[1] << std::endl;
                        exit( 1 );
                    }
                }
            }

            void calculate( char* buf )
            {
                std::size_t count = values_.size();

                if( count > 0 )
                {
                    comma::verbose << "calculating " << percentile_*100 << "th percentile using ";
                    T value;
                    typename std::multiset< T >::iterator it = values_.begin();
                    switch( method_ )
                    {
                        std::size_t rank;
                        
                        case nearest:
                            // https://en.wikipedia.org/wiki/Percentile#The_Nearest_Rank_method
                            comma::verbose << "nearest rank method" << std::endl;
                            comma::verbose << "see https://en.wikipedia.org/wiki/Percentile#The_Nearest_Rank_method" << std::endl;
                            rank = ( percentile_ == 0.0 ? 1 : std::ceil( count * percentile_ ));
                            comma::verbose << "n = " << rank << std::endl;
                            std::advance( it, rank - 1 );
                            value = *it;
                            break;

                        case interpolate:
                            // https://en.wikipedia.org/wiki/Percentile#The_Linear_Interpolation_Between_Closest_Ranks_method
                            // (third method in that section)
                            comma::verbose << "NIST linear interpolation method" << std::endl;
                            comma::verbose << "see http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm" << std::endl;
                            double x = percentile_ * ( count + 1 );
                            comma::verbose << "p = " << percentile_ << "; N = " << count
                                           << "; p(N + 1) = " << x;
                            if( x <= 1.0 ) {
                                comma::verbose << "; below 1 - choosing smallest value" << std::endl;
                                value = *it;
                            } else if( x >= count ) {
                                comma::verbose << "; above N - choosing largest value" << std::endl;
                                value = *( values_.rbegin() );
                            } else {
                                rank = x;
                                double remainder = x - rank;
                                comma::verbose << "; k = " << rank << "; d = " << remainder << std::endl;
                                std::advance( it, rank - 1 );
                                double v1 = *it;
                                double v2 = *++it;
                                value = v1 + ( v2 - v1 ) * remainder;
                                comma::verbose << "v1 = " << v1 << "; v2 = " << v2
                                               << "; result = " << value << std::endl;
                            }
                            break;
                    }
                    comma::csv::format::traits< T, F >::to_bin( static_cast< T >( value ), buf );
                }
            }

            base* clone() const { return new Percentile< T, F >( *this ); }

        private:
            std::multiset< T > values_;
            double percentile_;
            Method method_;
    };

    template < comma::csv::format::types_enum F >
    class Percentile< boost::posix_time::ptime, F > : public base
    {
        void push( const char* ) { COMMA_THROW( comma::exception, "percentile not implemented for time, todo" ); }
        void calculate( char* ) { COMMA_THROW( comma::exception, "percentile not implemented for time, todo" ); }
        base* clone() const { COMMA_THROW( comma::exception, "percentile not implemented for time, todo" ); }
    };

    template < typename T, comma::csv::format::types_enum F > class Stddev;
    template < typename T, comma::csv::format::types_enum F > class Variance;
    template < typename T, comma::csv::format::types_enum F > class Skew;
    template < typename T, comma::csv::format::types_enum F > class Kurtosis;
    template < typename T, unsigned int M > class Moment;
    template < typename T, unsigned int M > class moment_traits;
    
    
    // class for calculating 1st (mean), 2nd (variance), 3rd (skewness), 4th (kurtosis) moments
    // 
    // general formula combines the moments of two populations (e.g. M3A and M3B)
    // - this is the formula when population B contains only one element giving: n_A = (n - 1), n_B = 1, M2B, M3B, M4B = 0
    // d = x - M_1
    // M_1' = M_1 + d/n
    // M_2' = M_2 + d^2 (n - 1) / n
    // M_3' = M_3 + d^3 (n - 1)*(n - 2) + 3 d M_2 / n
    // M_4' = M_4 + d^4 (n - 1)( (n-1)^2 - (n - 1) + 1) / n^3 + 6 d^2 M_2 / n^2 - 4 d M_3 / n
    //
    // todo: refactor - there are many common terms (e.g d/n, d^2/n, d^2/n^2, d^3/n^2, d^4/n^3 ...)
    template < typename T >
    class moment_traits< T, 2 >
    {
    public:
        static typename result_traits< T >::type update( typename result_traits< T >::type d, std::size_t count, const Moment< T, 1 >& previous )
        {
            return d * d * (count - 1)/ count;
        }
    };
    
    template < typename T >
    class moment_traits< T, 3 >
    {
    public:
        static typename result_traits< T >::type update( typename result_traits< T >::type d,  std::size_t count, const Moment< T, 2>& previous )
        {
            return d * d * d * ( count - 1 ) * (count - 2) / count / count - 3 * d * previous.value() / count;
        }
    };
    
    template < typename T >
    class moment_traits< T, 4 >
    {
    public:
        static typename result_traits< T >::type update( typename result_traits< T >::type d,  std::size_t count, const Moment< T, 3 >& previous )
        {
            return   d * d * d * d / count / count / count * ( count - 1 ) * (count * count - 3 * count + 3) 
                   + 6 * d * d / count / count * previous.previous().value() 
                   - 4 * d / count * previous.value();
        }
    };
    
    template < typename T, unsigned int M >
    class Moment
    {
        public:
            Moment() : value_( 0 ), count_( 0 ) {}
            void update ( const T t )
            {
                typename result_traits< T >::type d = t - mean();
                ++count_;
                value_ = value_ + moment_traits< T, M >::update( d, count_, previous_ );
                previous_.update( t );
            }
            
            typename result_traits< T >::type value() const { return value_; }
            
            Moment< T, M - 1 > previous() const { return previous_; }
            
            std::size_t count() { return count_; }
            
            typename result_traits< T >::type mean() const { return previous_.mean(); }
            
        private:
            Moment< T, M - 1 > previous_;
            typename result_traits< T >::type value_;
            std::size_t count_;
    };
    
    template < typename T >
    class Moment< T, 1 >
    {
        public:
            Moment() : value_( 0 ), count_( 0 ) {}
            void update ( const T t )
            {   
                ++count_;
                value_ = value_ + ( t - value_ ) / count_;
            }
            
            typename result_traits< T >::type mean() const { return value_; }
            
        private:
            typename result_traits< T >::type value_;
            std::size_t count_;
    };
    
    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Stddev : public base
    {
        public:
            Stddev() : sample_ (false) {}
            void set_options( const std::vector< std::string >& options ) { sample_ = ( !options.empty() && options[0] == "sample" ); }
            void push( const char* buf ) 
            { 
                T t = comma::csv::format::traits< T >::from_bin( buf ); 
                if ( !first_ ) { first_ = t; }
                double diff = t - *first_;
                moments_.update( diff ); 
            }
            void update( const T t ) { moments_.update(t); }
            void calculate( char* buf ) { if( moments_.count() > 0 ) { comma::csv::format::traits< T, F >::to_bin( static_cast< T >( std::sqrt( static_cast< long double >( moments_.value() / ( sample_ ? moments_.count() - 1 : moments_.count() )  ) ) ), buf ); } }
            base* clone() const { return new Stddev< T, F >( *this ); }
        private:
            Moment< T, 2 > moments_;
            boost::optional<T> first_;
            bool sample_;
    };

    template < comma::csv::format::types_enum F >
    class Stddev< boost::posix_time::ptime, F > : public base
    {
        public:
            void set_options( const std::vector< std::string >& options ) { stddev_.set_options(options); }
            void push( const char* buf ) 
            {
                boost::posix_time::ptime t = comma::csv::format::traits< boost::posix_time::ptime >::from_bin( buf );
                if ( !first_ ) { first_ = t; }
                double diff = ( t - *first_ ).total_microseconds() / 1e6;
                stddev_.update(diff);
            }
            void calculate( char* buf ) { stddev_.calculate(buf); }
            base* clone() const { return new Stddev< boost::posix_time::ptime, F >( *this ); }
        private:
            Stddev< double, F > stddev_;
            boost::optional<boost::posix_time::ptime> first_;
    };
    
    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Variance : public base // todo: generalise for kth moment
    {
        public:
            Variance() : sample_ (false) {}
            void set_options( const std::vector< std::string >& options ) { sample_ = ( !options.empty() && options[0] == "sample" ); }
            void push( const char* buf ) 
            { 
                T t = comma::csv::format::traits< T >::from_bin( buf ); 
                if ( !first_ ) { first_ = t; }
                double diff = t - *first_;
                moments_.update( diff ); 
            }
            void update( const T t ) { moments_.update(t); }
            void calculate( char* buf ) { if( moments_.count() > 0 ) { comma::csv::format::traits< T, F >::to_bin( static_cast< T >( moments_.value() / ( sample_ ? moments_.count() - 1 : moments_.count() ) ), buf ); } }
            base* clone() const { return new Variance< T, F >( *this ); }
        private:
            Moment< T, 2 > moments_;
            boost::optional<T> first_;
            bool sample_;
    };

    template < comma::csv::format::types_enum F >
    class Variance< boost::posix_time::ptime, F > : public base
    {
        public:
            void set_options( const std::vector< std::string >& options ) { variance_.set_options(options); }
            void push( const char* buf ) 
            {
                boost::posix_time::ptime t = comma::csv::format::traits< boost::posix_time::ptime >::from_bin( buf );
                if ( !first_ ) { first_ = t; }
                double diff = ( t - *first_ ).total_microseconds() / 1e6;
                variance_.update( diff );
            }
            void calculate( char* buf ) { variance_.calculate(buf); }
            base* clone() const { return new Variance< boost::posix_time::ptime, F >( *this ); }
        private:
            Variance< double, F> variance_;
            boost::optional<boost::posix_time::ptime> first_;
    };
    
    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Skew : public base // todo: generalise for kth moment
    {
        public:
            void set_options( const std::vector< std::string >& options ) { sample_ = ( !options.empty() && options[0] == "sample" ); }
            void push( const char* buf ) 
            { 
                T t = comma::csv::format::traits< T >::from_bin( buf ); 
                if ( !first_ ) { first_ = t; }
                double diff = t - *first_;
                moments_.update( diff ); 
            }
            void update( const T t ) { moments_.update(t); }
            void calculate( char* buf ) 
            { 
                if( moments_.count() > 0 ) 
                { 
                    typename result_traits< T >::type n = moments_.count();
                    
                    // corrected sample skew requires at least 3 samples
                    typename result_traits< T >::type correction = sample_ ? sqrt( n * ( n - 1 ) ) / ( n - 2 ) : 1 ;
                    typename result_traits< T >::type m2 = moments_.previous().value();
                    typename result_traits< T >::type m3 = moments_.value();
                    comma::csv::format::traits< T, F >::to_bin( static_cast< T >( correction * sqrt( n / ( m2 * m2 * m2 ) ) * m3 ), buf ); 
                } 
            }
            base* clone() const { return new Skew< T, F >( *this ); }
        private:
            Moment< T, 3 > moments_;
            boost::optional<T> first_;
            bool sample_;
    };

    template < comma::csv::format::types_enum F >
    class Skew< boost::posix_time::ptime, F > : public base
    {
        public:
            void set_options( const std::vector< std::string >& options ) { skew_.set_options(options); }
            void push( const char* buf ) 
            {
                boost::posix_time::ptime t = comma::csv::format::traits< boost::posix_time::ptime >::from_bin( buf );
                if ( !first_ ) { first_ = t; }
                double diff = ( t - *first_ ).total_microseconds() / 1e6;
                skew_.update(diff);
            }
            void calculate( char* buf ) { skew_.calculate(buf); }
            base* clone() const { return new Skew< boost::posix_time::ptime, F >( *this ); }
        private:
            Skew< double, F> skew_;
            boost::optional<boost::posix_time::ptime> first_;
    };
    
    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Kurtosis : public base // todo: generalise for kth moment
    {
        public:
            Kurtosis(): sample_( false ), excess_( false ) {}
            void set_options( const std::vector< std::string >& options ) 
            { 
                for (std::size_t i = 0; i < options.size(); i++) 
                {
                    if ( options[i] == "sample" ) { sample_ = true; }
                    else if ( options[i] == "excess" ) { excess_ = true; }
                }
            }
            void push( const char* buf ) 
            { 
                T t = comma::csv::format::traits< T >::from_bin( buf ); 
                if ( !first_ ) { first_ = t; }
                double diff = t - *first_;
                moments_.update( diff ); 
            }
            void update( const T t ) { moments_.update(t); }
            void calculate( char* buf ) 
            { 
                if( moments_.count() > 0 ) 
                { 
                    typename result_traits< T >::type n = moments_.count();
                    typename result_traits< T >::type m2 = moments_.previous().previous().value();
                    typename result_traits< T >::type m4 = moments_.value();
                    typename result_traits< T >::type result = n * m4 / ( m2 * m2 );
                    
                    // corrected sample kurtosis requires at least 4 samples
                    if ( sample_ ) { result = n > 3 ? ( n - 1 ) / ( n - 2 ) / ( n - 3 ) * ( ( n + 1 ) * result - 3 * ( n - 1 ) ) + 3 : nan(""); }
                    if ( excess_ ) { result = result - 3; }
                    comma::csv::format::traits< T, F >::to_bin( static_cast< T >( result ), buf ); 
                } 
            }
            base* clone() const { return new Kurtosis< T, F >( *this ); }
        private:
            Moment< T, 4 > moments_;
            boost::optional<T> first_;
            bool sample_;
            bool excess_;
    };
    
    template < comma::csv::format::types_enum F >
    class Kurtosis< boost::posix_time::ptime, F > : public base
    {
        public:
            void set_options( const std::vector< std::string >& options ) { kurtosis_.set_options(options); }
            void push( const char* buf ) 
            {
                boost::posix_time::ptime t = comma::csv::format::traits< boost::posix_time::ptime >::from_bin( buf );
                if ( !first_ ) { first_ = t; }
                double diff = ( t - *first_ ).total_microseconds() / 1e6;
                kurtosis_.update(diff);
            }
            void calculate( char* buf ) { kurtosis_.calculate(buf); }
            base* clone() const { return new Kurtosis< boost::posix_time::ptime, F >( *this ); }
        private:
            Kurtosis< double, F> kurtosis_;
            boost::optional<boost::posix_time::ptime> first_;
    };
    
    template < typename T > struct Diff
    {
        typedef T Type;
        static Type subtract( T lhs, T rhs ) { return lhs - rhs; }
    };

    template <> struct Diff< boost::posix_time::ptime >
    {
        typedef double Type;
        static double subtract( boost::posix_time::ptime lhs, boost::posix_time::ptime rhs ) { return double( ( lhs - rhs ).total_microseconds() ) / 1e6; }
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Diameter : public base
    {
        public:
            void push( const char* buf ) { min_.push( buf ); max_.push( buf ); }
            void calculate( char* buf ) { if( min_.min_ ) { comma::csv::format::traits< typename Diff< T >::Type >::to_bin( Diff< T >::subtract( *max_.max_, *min_.min_ ), buf ); } }
            base* clone() const { return new Diameter< T, F >( *this ); }
        private:
            Min< T, F > min_;
            Max< T, F > max_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Radius : public base
    {
        public:
            void push( const char* buf ) { min_.push( buf ); max_.push( buf ); }
            void calculate( char* buf ) { if( min_.min_ ) { comma::csv::format::traits< typename Diff< T >::Type >::to_bin( Diff< T >::subtract( *max_.max_, *min_.min_ ) / 2, buf ); } }
            base* clone() const { return new Radius< T, F >( *this ); }
        private:
            Min< T, F > min_;
            Max< T, F > max_;
    };

    template < typename T, comma::csv::format::types_enum F = comma::csv::format::type_to_enum< T >::value >
    class Size : public base
    {
        public:
            Size() : count_( 0 ) {}
            void push( const char* ) { ++count_; }
            void calculate( char* buf ) { comma::csv::format::traits< comma::uint32 >::to_bin( count_, buf ); }
            base* clone() const { return new Size< T, F >( *this ); }
        private:
            std::size_t count_;
    };

    struct Enum { enum Values { min, max, mode, centre, mean, percentile, sum, size, radius, diameter, variance, stddev, skew, kurtosis }; };

    static Enum::Values from_name( const std::string& name )
    {
        if( name == "min" ) { return Enum::min; }
        else if( name == "max" ) { return Enum::max; }
        else if( name == "mode" ) { return Enum::mode; }
        else if( name == "centre" ) { return Enum::centre; }
        else if( name == "mean" ) { return Enum::mean; }
        else if( name == "percentile" ) { return Enum::percentile; }
        else if( name == "sum" ) { return Enum::sum; }
        else if( name == "radius" ) { return Enum::radius; }
        else if( name == "diameter" ) { return Enum::diameter; }
        else if( name == "var" ) { return Enum::variance; }
        else if( name == "stddev" ) { return Enum::stddev; }
        else if( name == "skew" ) { return Enum::skew; }
        else if( name == "kurtosis" ) { return Enum::kurtosis; }
        else if( name == "size" ) { return Enum::size; }
        else { COMMA_THROW( comma::exception, "expected operation name, got " << name ); }
    }

    struct operation_parameters
    {
        Enum::Values type;
        std::vector< std::string > options;
    };

    template < Enum::Values E > struct traits {};
    template <> struct traits< Enum::min > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Min< T, F > Type; }; };
    template <> struct traits< Enum::max > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Max< T, F > Type; }; };
    template <> struct traits< Enum::centre > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Centre< T, F > Type; }; };
    template <> struct traits< Enum::mean > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Mean< T, F > Type; }; };
    template <> struct traits< Enum::mode > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Mode< T, F > Type; }; };
    template <> struct traits< Enum::percentile > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Percentile< T, F > Type; }; };
    template <> struct traits< Enum::sum > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Sum< T, F > Type; }; };
    template <> struct traits< Enum::size > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Size< T, F > Type; }; };
    template <> struct traits< Enum::radius > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Radius< T, F > Type; }; };
    template <> struct traits< Enum::diameter > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Diameter< T, F > Type; }; };
    template <> struct traits< Enum::variance > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Variance< T, F > Type; }; };
    template <> struct traits< Enum::stddev > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Stddev< T, F > Type; }; };
    template <> struct traits< Enum::skew > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Skew< T, F > Type; }; };
    template <> struct traits< Enum::kurtosis > { template < typename T, comma::csv::format::types_enum F > struct FromEnum { typedef Kurtosis< T, F > Type; }; };
} // namespace Operations

class Operationbase
{
    public:
        virtual ~Operationbase() {}
        virtual void push( const char* buf ) = 0;
        virtual void calculate() = 0;
        virtual Operationbase* clone() const = 0;
        const comma::csv::format& output_format() const { return output_format_; }
        const char* buffer() const { return &buffer_[0]; }

    protected:
        boost::ptr_vector< Operations::base > operations_;
        comma::csv::format input_format_;
        std::vector< comma::csv::format::element > input_elements_;
        comma::csv::format output_format_;
        std::vector< comma::csv::format::element > output_elements_;
        std::vector< char > buffer_;

        Operationbase* deep_copy_to_( Operationbase* lhs ) const
        {
            lhs->input_format_ = input_format_;
            lhs->input_elements_ = input_elements_;
            lhs->output_format_ = output_format_;
            lhs->output_elements_ = output_elements_;
            lhs->buffer_ = buffer_;
            for( std::size_t i = 0; i < operations_.size(); ++i ) { lhs->operations_.push_back( operations_[i].clone() ); }
            return lhs;
        }
};

template < Operations::Enum::Values E >
struct Operation : public Operationbase
{
    Operation() {}
    Operation( const comma::csv::format& format
             , const std::vector< std::string >& options = std::vector< std::string >() )
    {
        input_format_ = format;
        input_elements_.reserve( input_format_.count() );
        for( std::size_t i = 0; i < input_format_.count(); ++i ) { input_elements_.push_back( input_format_.offset( i ) ); }
        operations_.reserve( input_elements_.size() );
        for( std::size_t i = 0; i < input_elements_.size(); ++i )
        {
            comma::csv::format::types_enum output_type = input_elements_[i].type;
            switch( E ) // quick and dirty, implement in operations::traits, just no time
            {
                case Operations::Enum::radius:
                case Operations::Enum::diameter:
                case Operations::Enum::stddev:
                case Operations::Enum::variance:
                case Operations::Enum::skew:
                case Operations::Enum::kurtosis:
                    if( input_elements_[i].type == comma::csv::format::time || input_elements_[i].type == comma::csv::format::long_time ) { output_type = comma::csv::format::double_t; }
                    break;
                case Operations::Enum::size:
                    output_type = comma::csv::format::uint32;
                    break;
                default:
                    break;
            }
            switch( input_elements_[i].type )
            {
                case comma::csv::format::char_t: operations_.push_back( new typename Operations::traits< E >::template FromEnum< char, comma::csv::format::char_t >::Type ); break;
                case comma::csv::format::int8: operations_.push_back( new typename Operations::traits< E >::template FromEnum< char, comma::csv::format::int8 >::Type ); break;
                case comma::csv::format::uint8: operations_.push_back( new typename Operations::traits< E >::template FromEnum< unsigned char, comma::csv::format::uint8 >::Type ); break;
                case comma::csv::format::int16: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::int16, comma::csv::format::int16 >::Type ); break;
                case comma::csv::format::uint16: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::uint16, comma::csv::format::uint16 >::Type ); break;
                case comma::csv::format::int32: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::int32, comma::csv::format::int32 >::Type ); break;
                case comma::csv::format::uint32: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::uint32, comma::csv::format::uint32 >::Type ); break;
                case comma::csv::format::int64: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::int64, comma::csv::format::int64 >::Type ); break;
                case comma::csv::format::uint64: operations_.push_back( new typename Operations::traits< E >::template FromEnum< comma::uint64, comma::csv::format::uint64 >::Type ); break;
                case comma::csv::format::float_t: operations_.push_back( new typename Operations::traits< E >::template FromEnum< float, comma::csv::format::float_t >::Type ); break;
                case comma::csv::format::double_t: operations_.push_back( new typename Operations::traits< E >::template FromEnum< double, comma::csv::format::double_t >::Type ); break;
                case comma::csv::format::time: operations_.push_back( new typename Operations::traits< E >::template FromEnum< boost::posix_time::ptime, comma::csv::format::time >::Type ); break;
                case comma::csv::format::long_time: operations_.push_back( new typename Operations::traits< E >::template FromEnum< boost::posix_time::ptime, comma::csv::format::long_time >::Type ); break;
                default: COMMA_THROW( comma::exception, "operations for " << i << "th element in " << format.string() << " not defined" );
            }
            // Call set_options() on the operation that we just added
            operations_[ operations_.size() - 1 ].set_options( options );
            operations_.back().set_options( options );

            output_format_ += comma::csv::format::to_format( output_type );
        }
        for( std::size_t i = 0; i < input_elements_.size(); ++i ) { output_elements_.push_back( output_format_.offset( i ) ); }
        buffer_.resize( output_format_.size() );
    }

    void push( const char* buf )
    {
        for( std::size_t i = 0; i < operations_.size(); ++i ) { operations_[i].push( buf + input_elements_[i].offset ); }
    }

    void calculate()
    {
        for( std::size_t i = 0; i < operations_.size(); ++i ) { operations_[i].calculate( &buffer_[0] + output_elements_[i].offset ); }
    }

    Operationbase* clone() const { Operation< E >* op = new Operation< E >; return deep_copy_to_( op ); }
};

typedef boost::unordered_map< comma::uint32, boost::ptr_vector< Operationbase >* > OperationsMap;
typedef boost::unordered_map< comma::uint32, std::string > ResultsMap;
typedef std::vector< std::pair < comma::uint32, std::string > > Inputs;

static void init_operations( boost::ptr_vector< Operationbase >& operations
                           , const std::vector< Operations::operation_parameters >& operations_parameters
                           , const comma::csv::format& format )
{
    static boost::ptr_vector< Operationbase > sample;
    if( sample.empty() )
    {
        sample.reserve( operations_parameters.size() );
        for( std::size_t i = 0; i < operations_parameters.size(); ++i )
        {
            switch( operations_parameters[i].type )
            {
                case Operations::Enum::min: sample.push_back( new Operation< Operations::Enum::min >( format ) ); break;
                case Operations::Enum::max: sample.push_back( new Operation< Operations::Enum::max >( format ) ); break;
                case Operations::Enum::centre: sample.push_back( new Operation< Operations::Enum::centre >( format ) ); break;
                case Operations::Enum::mean: sample.push_back( new Operation< Operations::Enum::mean >( format ) ); break;
                case Operations::Enum::mode: sample.push_back( new Operation< Operations::Enum::mode >( format ) ); break;
                case Operations::Enum::percentile: sample.push_back( new Operation< Operations::Enum::percentile >( format, operations_parameters[i].options ) ); break;
                case Operations::Enum::radius: sample.push_back( new Operation< Operations::Enum::radius >( format ) ); break;
                case Operations::Enum::diameter: sample.push_back( new Operation< Operations::Enum::diameter >( format ) ); break;
                case Operations::Enum::variance: sample.push_back( new Operation< Operations::Enum::variance >( format, operations_parameters[i].options ) ); break;
                case Operations::Enum::stddev: sample.push_back( new Operation< Operations::Enum::stddev >( format, operations_parameters[i].options ) ); break;
                case Operations::Enum::skew: sample.push_back( new Operation< Operations::Enum::skew >( format, operations_parameters[i].options ) ); break;
                case Operations::Enum::kurtosis: sample.push_back( new Operation< Operations::Enum::kurtosis >( format, operations_parameters[i].options ) ); break;
                case Operations::Enum::sum: sample.push_back( new Operation< Operations::Enum::sum >( format ) ); break;
                case Operations::Enum::size: sample.push_back( new Operation< Operations::Enum::size >( format ) ); break;
            }
        }
    }
    operations.clear();
    for( std::size_t i = 0; i < sample.size(); ++i ) { operations.push_back( sample[i].clone() ); }
}

static void output( const comma::csv::options& csv, ResultsMap& results, boost::optional< comma::uint32 > block, bool has_block, bool has_id )
{
    for( ResultsMap::iterator it = results.begin(); it != results.end(); ++it )
    {
        std::cout << it->second;
        if( csv.binary() )
        {
            if( has_id )  { std::cout.write( reinterpret_cast< const char* >( &it->first ), sizeof( comma::uint32 ) ); } // quick and dirty
            if( has_block ) { std::cout.write( reinterpret_cast< const char* >( &( *block ) ), sizeof( comma::uint32 ) ); } // quick and dirty
            std::cout.flush();
        }
        else
        {
            if( has_id ) { std::cout << csv.delimiter << it->first; }
            if( has_block ) { std::cout << csv.delimiter << *block; }
            std::cout << std::endl;
        }
    }
    results.clear();
}

static void append_and_output( const comma::csv::options& csv, Inputs& inputs, ResultsMap& results )
{    
    for ( size_t i = 0; i < inputs.size(); ++i )
    {
        std::cout << inputs[i].second;
        if (!csv.binary()) { std::cout << csv.delimiter; }
        std::cout << results.find(inputs[i].first)->second;
        if (!csv.binary()) { std::cout << std::endl; }
    }
    results.clear();
    inputs.clear();
}

static void calculate( const comma::csv::options& csv, OperationsMap& operations, ResultsMap& results )
{
    for( OperationsMap::iterator it = operations.begin(); it != operations.end(); ++it )
    {
        std::string r;
        for( std::size_t i = 0; i < it->second->size(); ++i )
        {
            ( *it->second )[i].calculate();
            if( csv.binary() ) { r.append( ( *it->second )[i].buffer(), ( *it->second )[i].output_format().size() ); }
            else { if( i > 0 ) { r += csv.delimiter; } r.append(( *it->second )[i].output_format().bin_to_csv( ( *it->second )[i].buffer(), csv.delimiter, 12 )); }
        }
        results[it->first] = r;
    }
    for( OperationsMap::iterator it = operations.begin(); it != operations.end(); ++it ) { delete it->second; } // quick and dirty
    operations.clear();
}

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av, usage );
        if( options.exists( "--bash-completion" ) ) bash_completion( ac, av );
        std::vector< std::string > unnamed = options.unnamed( "", "--binary,-b,--delimiter,-d,--format,--fields,-f,--output-fields" );
        comma::csv::options csv( options );
        #ifdef WIN32
        if( csv.binary() ) { _setmode( _fileno( stdin ), _O_BINARY ); _setmode( _fileno( stdout ), _O_BINARY ); }
        #endif
        if( unnamed.empty() ) { std::cerr << comma::verbose.app_name() << ": please specify operations" << std::endl; exit( 1 ); }
        std::vector< std::string > v = comma::split( unnamed[0], ',' );
        std::vector< Operations::operation_parameters > operations_parameters( v.size() );
        for( std::size_t i = 0; i < v.size(); ++i )
        {
            std::vector< std::string > p = comma::split( v[i], '=' );
            operations_parameters[i].type = Operations::from_name( p[0] );
            if( p.size() == 2 ){ operations_parameters[i].options = comma::split( p[1], ':' ); }
        }
        boost::optional< comma::csv::format > format;
        if( csv.binary() ) { format = csv.format(); }
        else if( options.exists( "--format" ) ) { format = comma::csv::format( options.value< std::string >( "--format" ) ); }
        boost::scoped_ptr< ascii_input > ascii;
        boost::scoped_ptr< binary_input > binary;
        if( csv.binary() ) { binary.reset( new binary_input( csv ) ); }
        else { ascii.reset( new ascii_input( csv, format ) ); }
        OperationsMap operations;
        ResultsMap results;
        Inputs inputs;
        boost::optional< comma::uint32 > block = boost::make_optional< comma::uint32 >( false, 0 );
        bool has_block = csv.has_field( "block" );
        bool has_id = csv.has_field( "id" );
        bool append = options.exists("--append");
        
        if( options.exists( "--output-fields" ) )
        {
            std::vector < std::string > fields = comma::split(csv.fields, ',');
            std::vector < std::string > output_fields;
            for (std::size_t op = 0; op < v.size(); op++)
            {
                std::replace(v[op].begin(), v[op].end(), '=', '_');
                std::replace(v[op].begin(), v[op].end(), '.', '_');
                std::replace(v[op].begin(), v[op].end(), ':', '_');
                for (std::size_t f = 0; f < fields.size(); f++ )
                {
                    if (fields[f] == "" || fields[f] == "id" || fields[f] == "block") { continue; }
                    output_fields.push_back(fields[f] + "/" + v[op]);
                }
            }
            if (has_id && !append) { output_fields.push_back("id"); }
            if (has_block && !append ) { output_fields.push_back("block"); }
            std::cout << comma::join(output_fields, ',') << std::endl;
            return 0;
        }
        if (options.exists("--output-format"))
        {
            if ( !format ) { std::cerr << comma::verbose.app_name() << ": option --output-format requires input format to be specified, please use --format or --binary" << std::endl; return 1; }
            boost::ptr_vector< Operationbase > ops;
            init_operations(ops, operations_parameters, Values(csv, *format).format());
            for ( std::size_t i = 0; i < ops.size(); ++i ) 
            { 
                if ( i > 0 ) { std::cout << csv.delimiter; }
                std::cout << ops[i].output_format().string();
            }
            if (has_id && !append) { std::cout << csv.delimiter << "ui"; }
            if (has_block && !append) { std::cout << csv.delimiter << "ui"; }
            std::cout << std::endl;
            return 0;
        } 
        while( std::cin.good() && !std::cin.eof() )
        {
            const Values* v = csv.binary() ? binary->read() : ascii->read();
            if( v == NULL ) { if( csv.binary() ) { break; } else { continue; } } // quick and dirty: skip empty lines in ascii
            if( has_block )
            {
                if( block && *block != v->block() ) 
                { 
                    calculate(csv, operations, results);
                    if ( append ) { append_and_output(csv, inputs, results); inputs.clear(); }
                    else { output( csv, results, block, has_block, has_id ); }
                }
                block = v->block();
            }
            OperationsMap::iterator it = operations.find( v->id() );
            if( it == operations.end() )
            {
                it = operations.insert( std::make_pair( v->id(), new boost::ptr_vector< Operationbase > ) ).first;
                init_operations( *it->second, operations_parameters, v->format() );
            }
            if (append) { inputs.push_back( std::make_pair( v->id(), csv.binary() ? binary->line() : ascii->line() ) ); }
            for( std::size_t i = 0; i < it->second->size(); ++i ) { ( *it->second )[i].push( v->buffer() ); }
        }
        calculate(csv, operations, results);
        if ( append ) { append_and_output(csv, inputs, results); }
        else { output( csv, results, block, has_block, has_id ); }
        return 0;
    }
    catch( std::exception& ex ) { std::cerr << comma::verbose.app_name() << ": " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << comma::verbose.app_name() << ": unknown exception" << std::endl; }
    return 1;
}

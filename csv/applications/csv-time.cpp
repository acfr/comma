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
/// @author mathew hounsell

#include <algorithm>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

#include <comma/application/contact_info.h>
#include <comma/application/command_line_options.h>
#include <comma/base/exception.h>
#include <comma/csv/stream.h>
#include <comma/visiting/traits.h>

#include <comma/base/types.h>
#include <comma/string/string.h>
#include <comma/csv/impl/epoch.h>

static void usage()
{
    static char const * const msg_general =
        "\n"
        "\nConvert between a couple of common time representation"
        "\n"
        "\nUsage:"
        "\n    cat log.csv | csv-time <options> > converted.csv"
        "\n"
        "\nOptions"
        "\n    --from <what>: input format: any, iso, seconds, sql, aixm; default iso"
        "\n    --to <what>: output format: iso, seconds, sql, aixm; default iso"
        "\n    --delimiter,-d <delimiter> : default: ','"
        "\n    --fields <fields> : time field names or field numbers as in \"cut\""
        "\n                        e.g. \"1,5,7\" or \"a,b,,d\""
        "\n                        n.b. use field names to skip transforming a field"
        "\n"
        "\nTime formats"
        "\n    - iso, iso-8601-basic"
        "\n            YYYYMMDDTHHMMSS.FFFFFF, e.g. 20140101T001122.333"
        "\n    - sql, posix, ieee-std-1003.1"
        "\n            e.g. 2014-01-01 00:11:22"
        "\n    - xsd, iso-8601-extended"
        "\n            used in xsd:dateTime, xs:dateTime, gml and derivatives"
        "\n            e.g. 2014-12-25T00:00:00.000Z"
        "\n            e.g. 2014-12-25T00:00:00.000+11:00"
        "\n    - seconds"
        "\n            seconds since UNIX epoch as double"
        "\n    - any, guess"
        "\n            a special input format - try to convert from all those supported,"
        "\n            default input format, will be slower"
        "\n"
        "\nDeprecated Options:"
        "\n    --to-seconds,--sec,-s: iso input expected; use --from, --to"
        "\n    --to-iso-string,--iso,-i: input as seconds expected; use --from, --to"
        "\n"
        "\n";
    std::cerr << msg_general << comma::contact_info << '\n' << std::endl;
    exit( 1 );
}

enum what_t { guess, iso, seconds, sql, aixm };
static what_t from = guess;
static what_t to = iso;

std::string to_string( const boost::posix_time::ptime& t, what_t w );

static what_t what( const std::string& option, const comma::command_line_options& options )
{
    std::string s = options.value< std::string >( option, "iso" );
    if(! s.empty() )
    {
        if( 'i' == s[0] )
        {
            if( "iso" == s ) return iso;
            if( "iso-8601-basic" == s ) return iso;
            if( "iso-8601-extended" == s ) { return aixm; }
            if( "ieee-std-1003.1" == s ) return sql;
        }
        else if( 'p' == s[0] )
        {
            if( "posix" == s ) return sql;
        }
        else if( 's' == s[0] )
        {
            if( "sql" == s ) return sql;
            if( "seconds" == s ) return seconds;
        }
        else if( 'x' == s[0] )
        {
            if( "xsd" == s ) { return aixm; }
        }
        else if( 'g' == s[0] )
        {
            if( "guess" == s ) { return guess; }
        }
        else if( 'a' == s[0] )
        {
            if( "any" == s ) { return guess; }
        }
    }
    std::cerr << "csv-time: expected seconds, sql, or iso; got: \"" << s << "\"" << std::endl;
    exit( 1 );
}

bool from_string_actual_aixm( const std::string& s, const what_t w, boost::posix_time::ptime & out_time )
{
    std::string t( s );
    size_t const idx_t = t.find( 'T' );
    if ( std::string::npos != idx_t ) t[idx_t] = ' ';
    size_t const idx_z = t.size() - 1;
    if ( 'Z' == t[idx_z] ) t.erase( idx_z );
    if ( ! ( t.size() > 6 && ( '+' == t[t.size() - 6] || '-' == t[t.size() - 6] ) ) )
    {
        out_time = boost::posix_time::time_from_string( t );
    }
    else
    {
        signed multiple = ( '-' == t[t.size() - 6] ) ? 1 : -1; // multiple is the reverse of the sign
        signed hrs = multiple * boost::lexical_cast<unsigned>(&t[t.size() - 5], 2);
        if (hrs < -12 || hrs > 12 ) COMMA_THROW( comma::exception, "hours must be [0..12]" );
        signed mins = multiple * boost::lexical_cast<unsigned>(&t[t.size() - 2], 2);
        if (mins < -60 || mins > 60 ) COMMA_THROW( comma::exception, "minutes must be [0..60]" );
        t.resize( t.size() - 6 );
        out_time = boost::posix_time::time_from_string( t );
        out_time += boost::posix_time::hours(hrs) + boost::posix_time::minutes(mins);
    }
    return ! out_time.is_not_a_date_time();
}

bool from_string_actual( const std::string& s, const what_t w, boost::posix_time::ptime & out_time )
{
    switch( w )
    {
        case iso:
        {
            if( s == boost::posix_time::to_iso_string( boost::posix_time::ptime() ) )
                return false;
                
            out_time = boost::posix_time::from_iso_string( s );
            return ! out_time.is_not_a_date_time();
        }

        case seconds:
        {
            double d = boost::lexical_cast< double >( s );
            long long seconds = d;
            int microseconds = ( d - seconds ) * 1000000;
            out_time = boost::posix_time::ptime( comma::csv::impl::epoch, boost::posix_time::seconds( seconds ) + boost::posix_time::microseconds( microseconds ) );
            return ! out_time.is_not_a_date_time();
        }

        case sql:
        {
            if( s == "NULL" || s == "null" )
                return false;
            out_time = boost::posix_time::time_from_string( s );
            return ! out_time.is_not_a_date_time();
        }

        case aixm: // 2014-03-05T23:00:00.000Z
            return from_string_actual_aixm( s, w, out_time );
        
        case guess:
            COMMA_THROW( comma::exception, "never guess" );
    }
    COMMA_THROW( comma::exception, "never here" );
}

bool from_string_forgiving( const std::string& s, const what_t w, boost::posix_time::ptime & out_time )
{
    try {
        from_string_actual( s, w, out_time );
        // std::cerr << "Decoded '" << s << "' as ISO '" << to_string( out_time, iso ) << '\'' << std::endl;
        return true;
    } catch (...) { }
    return false;
}

bool from_string( const std::string& s, const what_t w, boost::posix_time::ptime & out_time  )
{
    if ( s.empty() ) COMMA_THROW( comma::exception, "no input" );

    if ( guess != w )
        return from_string_actual( s, w, out_time );

    // order is important
    if ( from_string_forgiving( s, iso, out_time ) ) return true;
    if ( from_string_forgiving( s, aixm, out_time ) ) return true;
    if ( from_string_forgiving( s, sql, out_time ) ) return true;
    if ( from_string_forgiving( s, seconds, out_time ) ) return true;
    return false;
}

std::string to_string( const boost::posix_time::ptime& t, what_t w )
{
    switch( w )
    {
        case iso:
            return boost::posix_time::to_iso_string( t );

        case seconds: // quick and dirty
        {
            const boost::posix_time::ptime base( comma::csv::impl::epoch );
            const boost::posix_time::time_duration d = t - base;
            comma::int64 seconds = d.total_seconds();
            comma::int32 nanoseconds = ( d - boost::posix_time::seconds( seconds ) ).total_microseconds() * 1000;
            std::ostringstream oss;
            oss << ( seconds == 0 && nanoseconds < 0 ? "-" : "" ) << seconds;
            if( nanoseconds != 0 )
            {
                oss << '.';
                oss.width( 9 );
                oss.fill( '0' );
                oss << std::abs( nanoseconds );
            }
            return oss.str();
        }

        case sql:
            return t.is_not_a_date_time() ? std::string( "NULL" ) : comma::split( boost::replace_all_copy( boost::posix_time::to_iso_extended_string( t ), "T", " " ), '.' )[0];

        case aixm: // 2014-03-05T23:00:00.000Z
            return boost::posix_time::to_iso_extended_string( t );

        case guess:
            COMMA_THROW( comma::exception, "never guess" );
    }
    COMMA_THROW( comma::exception, "never here" );
}

struct input_t { std::vector< std::string > values; };

namespace comma { namespace visiting {

template <> struct traits< input_t >
{
    template < typename K, typename V > static void visit( const K&, const input_t& p, V& v )
    {
        v.apply( "values", p.values );
    }

    template < typename K, typename V > static void visit( const K&, input_t& p, V& v )
    {
        v.apply( "values", p.values );
    }
};

} } // namespace comma { namespace visiting {

static bool verbose;
static comma::csv::options csv;
static input_t input;

static void init_input()
{
    const std::vector< std::string >& original_names = comma::split( csv.fields, ',' );

    std::vector< std::string > names;
    names.reserve( original_names.size() );
    for( unsigned i = 0; i < original_names.size(); ++i )
        names.push_back( comma::strip( original_names[i], ' ' ) );
    
    std::vector< bool > keep;
    
    bool legacy = true;
    for( unsigned i = 0; i < names.size() && legacy; ++i )
    {
        if( names[i].empty() )
        {
            legacy = false;
            break;
        }
        try {
            const unsigned idx = boost::lexical_cast< unsigned >( names[i] );
            if ( keep.size() < idx ) keep.resize(idx, false);
            keep[idx - 1] = true;
        } catch( ... ) {
            legacy = false;
        }
    }
    
    std::string fields;
    std::string comma;
    unsigned size = 0;
    if ( legacy )
    {
        for( unsigned i = 0; i < keep.size(); ++i )
        {
            fields += comma;
            comma = ",";
            if( keep[i] )
                fields += "values[" + boost::lexical_cast< std::string >( size++ ) + "]";
        }
    }
    else
    {
        for( unsigned i = 0; i < names.size(); ++i )
        {
            fields += comma;
            comma = ",";

            if( ! names[i].empty() )
                fields += "values[" + boost::lexical_cast< std::string >( size++ ) + "]";
        }
    }

    csv.fields = fields;
    csv.full_xpath = true;
    input.values.resize( size );
}

static int run()
{
    comma::csv::input_stream< input_t > istream( std::cin, csv, input );
    comma::csv::output_stream< input_t > ostream( std::cout, csv, input );

    std::string const failed_text( to_string( boost::posix_time::ptime(), to ) );
    
    while( istream.ready() || ( std::cin.good() && !std::cin.eof() ) )
    {
        const input_t* p = istream.read();
        if( !p ) { break; }
        input_t output = *p;
        
        for( unsigned int i = 0; i < output.values.size(); ++i )
        {
            boost::posix_time::ptime t( boost::posix_time::not_a_date_time );
            if( ! from_string( output.values[i], from, t ) )
                output.values[i] = failed_text;
            else if( t.is_not_a_date_time() )
                output.values[i] = failed_text;
            else
                output.values[i] = to_string( t, to );
        }

        ostream.write( output, istream );
    }
    return 0;
}

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av );
        if( options.exists( "--help" ) || options.exists( "-h" ) || ac == 1 ) { usage(); }
        verbose = options.exists( "--verbose,-v" );
        csv = comma::csv::options( options );
        if( csv.fields.empty() ) { csv.fields="a"; }
        init_input();

        options.assert_mutually_exclusive( "--to-seconds,--to-iso-string,--seconds,--sec,--iso,--aixm,-s,-i,--from" );
        options.assert_mutually_exclusive( "--to-seconds,--to-iso-string,--seconds,--sec,--iso,--aixm,-s,-i,--to" );

        if( options.exists( "--to-iso-string,--iso,-i" ) ) { from = seconds; to = iso; }
        else if ( options.exists( "--to-seconds,--sec,-s" ) ) { from = iso; to = seconds; }
        else { from = what( "--from", options ); to = what( "--to", options ); }
        
        if( guess == to ) { std::cerr << "csv-time: specify an actual output format; not any." << std::endl; return 1; }

        return run();
    }
    catch( std::exception& ex ) { std::cerr << "csv-time: " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << "csv-time: unknown exception" << std::endl; }
    return 1;
}

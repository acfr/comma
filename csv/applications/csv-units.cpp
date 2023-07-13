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
/// @author kai huang

#include <iostream>
#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/unordered/unordered_map.hpp>

#include <boost/units/systems/si.hpp>
#include <boost/units/systems/si/length.hpp>
#include <boost/units/systems/si/mass.hpp>

#include <boost/units/systems/si/temperature.hpp>
#include <boost/units/systems/temperature/celsius.hpp>
#include <boost/units/systems/temperature/fahrenheit.hpp>

#include <boost/units/base_units/us/foot.hpp>
#include <boost/units/base_units/us/pound.hpp>
#include <boost/units/base_units/metric/nautical_mile.hpp>
#include <boost/units/base_units/us/mile.hpp>
#include <boost/units/base_units/metric/knot.hpp>
#include <boost/units/base_units/angle/radian.hpp>
#include <boost/units/base_units/angle/degree.hpp>

#include "../../application/command_line_options.h"
#include "../../application/contact_info.h"
#include "../../base/exception.h"
#include "../../csv/stream.h"
#include "../../visiting/traits.h"

/// Outputs the usage syntax, options and examples
/// @warning exits the program with an error code of 1
static void usage(char const * const txt = "")
{
    static char const * const msg_general =
        "\n"
        "\nconvert units in a file or stream by specifying the conversion units and the csv fields to be converted"
        "\n"
        "\nUsage: cat a.csv | csv-units <options>"
        "\n"
        "\nOptions:"
        "\n    --from <unit>   : unit converting from; default: metric, unless units specified in --fields"
        "\n    --to   <unit>   : unit converting to; default: metric, unless units specified in --fields"
        "\n    --scale <factor> : scale value by given factor instead of unit conversion"
        "\n                       a convenience option, probably somewhat misplaced"
        "\n    --offset <value> : offset each value by a given <value> instead of unit conversion"
        "\n                       a convenience option, probably somewhat misplaced"
        "\n"
        "\nSupported Units:"
        "\n    metres / feet / statute-miles / nautical-miles "
        "\n    kilograms / pounds"
        "\n    meters-per-second / knots"
        "\n    kelvin / celsius / fahrenheit"
        "\n    radians / degrees"
        "\n    hours / minutes / seconds"
        "\n    percent / fraction"
        "\n"
        "\n    any case is supported and so are abbreviations"
        "\n         meters, metres, m"
        "\n         feet, ft"
        "\n         statute-miles, miles, mi"
        "\n         nautical-miles, nm"
        "\n         radians, rad"
        "\n         degrees, deg"
        "\n         hours"
        "\n         minuts, min"
        "\n         seconds, sec"
        "\n         percent"
        "\n         fraction"
        "\n"
        "\ndata driven"
        "\n    This program can be configured to read the --from units from the input data."
        "\n    End a field name with 'units', to have it treated as --from for a field."
        "\n    You can explicitly or implicitly set value for a data field."
        "\n    So for example you can specify x or x/value with x/units to drive --from by the data.";
    static char const * const msg_examples =
        "\n"
        "\nexamples"
        "\n    echo 1.2345 | csv-units --from meters --to feet "
        "\n    echo 1.2345,2.3456 | csv-units --from kilograms --to pounds --fields=a,b"
        "\n    echo 1,pounds,2,kilograms | csv-units --to pounds --fields x,x/units,y,y/units";

    if( 0 != txt[0] ) std::cerr << "error: " << txt << std::endl;
    std::cerr << msg_general << std::endl; // endl to make this function easier to debug by flushing
    std::cerr << "\ncsv options\n" << comma::csv::options::usage() << std::endl;
    std::cerr << msg_examples << std::endl;
    std::cerr << '\n' << comma::contact_info << '\n' << std::endl;
    exit( 1 );
}

// Specify some aliases to represent the measurement units supported.
typedef boost::units::si::length::unit_type length_t;
typedef boost::units::us::foot_base_unit::unit_type imperial_us_length_t;
typedef boost::units::metric::nautical_mile_base_unit::unit_type nautical_mile_t;
typedef boost::units::us::mile_base_unit::unit_type statute_mile_t;
typedef boost::units::si::velocity::unit_type velocity_t;
typedef boost::units::metric::knot_base_unit::unit_type knot_t;
typedef boost::units::si::kilogram_base_unit::unit_type mass_t;
typedef boost::units::us::pound_base_unit::unit_type imperial_us_mass_t;
typedef boost::units::angle::radian_base_unit::unit_type radian_t;
typedef boost::units::angle::degree_base_unit::unit_type degree_t;
typedef boost::units::absolute< boost::units::si::temperature > kelvin_t;
typedef boost::units::absolute< boost::units::celsius::temperature > celsius_t;
typedef boost::units::absolute< boost::units::fahrenheit::temperature > fahrenheit_t;

/// Converts the given value between the two template measurement units 
template < typename From, typename To >
double cast( double input )
{
    typedef boost::units::quantity< From > from_quantity_t;
    typedef boost::units::quantity< To > to_quantity_t;
    return static_cast< to_quantity_t >( from_quantity_t( input * From() ) ).value();
}

static double scale( double input, double factor ) { return input * factor; }

static double null_cast( double input ) { return input; }
template double cast< imperial_us_mass_t, mass_t >( double );
template double cast< mass_t, imperial_us_mass_t >( double );
template double cast< imperial_us_length_t, nautical_mile_t >( double );
template double cast< imperial_us_length_t, statute_mile_t >( double );
template double cast< imperial_us_length_t, length_t >( double );
template double cast< length_t, nautical_mile_t >( double );
template double cast< length_t, statute_mile_t >( double );
template double cast< length_t, imperial_us_length_t >( double );
template double cast< nautical_mile_t, imperial_us_length_t >( double );
template double cast< nautical_mile_t, statute_mile_t >( double );
template double cast< nautical_mile_t, length_t >( double );
template double cast< statute_mile_t, imperial_us_length_t >( double );
template double cast< statute_mile_t, nautical_mile_t >( double );
template double cast< statute_mile_t, length_t >( double );
template double cast< velocity_t, knot_t >( double );
template double cast< knot_t, velocity_t >( double );
template double cast< radian_t, degree_t >( double );
template double cast< degree_t, radian_t >( double );
template double cast< kelvin_t, fahrenheit_t >( double );
template double cast< kelvin_t, celsius_t >( double );
template double cast< celsius_t, fahrenheit_t >( double );
template double cast< celsius_t, kelvin_t >( double );
template double cast< fahrenheit_t, kelvin_t >( double );
template double cast< fahrenheit_t, celsius_t >( double );

static std::string to_lower( const std::string& s )
{
    std::string t = s;
    for( unsigned int i = 0; i < s.size(); ++i ) { if( s[i] >= 'A' && s[i] <= 'Z' ) { t[i] = s[i] - 'A' + 'a'; } }
    return t;
}

// A name space to wrap the manipulation of named conversions.
namespace units {
    // A set of number to identify the supported measurement units.
    enum et { celsius = 0,
              degrees,
              fahrenheit,
              feet,
              kelvin,
              kilograms,
              knots,
              metres,
              metres_per_second,
              nautical_miles,
              pounds,
              radians,
              statute_miles,
              hours,
              minutes,
              seconds,
              percent,
              fraction,
              count,
              invalid
    };
    
    static boost::array< et, count > metric = {{ kelvin
                                               , radians
                                               , kelvin
                                               , metres
                                               , kelvin
                                               , kilograms
                                               , metres_per_second
                                               , metres
                                               , metres_per_second
                                               , metres
                                               , kilograms
                                               , radians
                                               , metres
                                               , seconds
                                               , seconds
                                               , seconds
                                               , fraction
                                               , fraction }};

    /// Retrieve a human readable canonical name for the given number. Supports
    /// the extra two internal numbers (count and invalid) for diagnostics.
    char const * name( const et val )
    {
        if ( val < 0 || val > invalid ) { COMMA_THROW( comma::exception, "can not get name for invalid units " << val ); }
        static char const * const names[count + 2]
            = { "celsius",
                "degrees",
                "fahrenheit",
                "feet",
                "kelvin",
                "kilograms",
                "knots",
                "metres",
                "metres_per_second",
                "nautical_miles",
                "pounds",
                "radians",
                "statute_miles",
                "hours",
                "minutes",
                "seconds",
                "percent",
                "fraction",
                "count",
                "invalid"
            };
        return names[val];
    }

    /// returns a name even if the value is invalid
    std::string debug_name( const et val ) { return val < 0 || val > invalid ? "ERROR:" + boost::lexical_cast<std::string>(val) : name( val ); }

    /// Given a canonical name or an alias of a measurement unit 
    /// retrieve the canonical enumeration.
    et value( std::string const & str )
    {
        typedef boost::unordered_map<std::string, et> map_t;
        static map_t map;
        if ( map.empty() )
        {
            map["celsius"] = celsius;
            map["deg"] = degrees;
            map["degrees"] = degrees;
            map["fahrenheit"] = fahrenheit;
            map["feet"] = feet;
            map["ft"] = feet;
            map["kelvin"] = kelvin;
            map["kg"] = kilograms;
            map["kilograms"] = kilograms;
            map["knots"] = knots;
            map["lbs"] = pounds;
            map["meters"] = metres;
            map["meters-per-second"] = metres_per_second;
            map["metres-per-second"] = metres_per_second;
            map["metres"] = metres;
            map["miles"] = statute_miles;
            map["mi"] = statute_miles;
            map["m"] = metres;
            map["nautical-miles"] = nautical_miles;
            map["nm"] = nautical_miles;
            map["pounds"] = pounds;
            map["radians"] = radians;
            map["rad"] = radians;
            map["statute-miles"] = statute_miles;
            map["hours"] = hours;
            map["minutes"] = minutes;
            map["min"] = minutes;
            map["seconds"] = seconds;
            map["sec"] = seconds;
            map["percent"] = percent;
            map["fraction"] = fraction;
        }
        map_t::const_iterator const citr = map.find( to_lower(str) );
        if( map.cend() != citr ) { return citr->second; }
        COMMA_THROW( comma::exception, "expected unit name, got \"" << str << "\"" );
    }
    
    /// A type to allow a lookup table for converting units
    //typedef double (* cast_function)( double );
    typedef boost::function< double( double ) > cast_function;
    
    /// Retrieve a function that will convert between the two given
    /// measurement units.
    /// @returns NULL if the conversion is not supported.
    cast_function cast_lookup( const et from, const et to ) // quick and dirty
    {
        if ( from < 0 || from >= count ) { COMMA_THROW( comma::exception, "can not cast lookup for invalid unit (from) " << from ); }
        if ( to < 0 || to >= count ) { COMMA_THROW( comma::exception, "can not cast lookup for invalid unit (to) " << to ); }        
        static cast_function map[count][count] = { { NULL, }, };
        static bool initialised = false;
        if (! initialised )
        {
            initialised = true;
            for( unsigned int i = 0; i < count; ++i ) { map[i][i] = null_cast; }
            map[pounds][kilograms] = cast< imperial_us_mass_t,mass_t >;
            map[kilograms][pounds] = cast< mass_t,imperial_us_mass_t >;
            map[metres_per_second][knots] = cast< velocity_t, knot_t >;
            map[knots][metres_per_second] = cast< knot_t, velocity_t >;
            map[radians][degrees] = cast< radian_t, degree_t >;
            map[degrees][radians] = cast< degree_t, radian_t >;
            map[kelvin][celsius] = cast< kelvin_t, celsius_t >;
            map[kelvin][fahrenheit] = cast< kelvin_t, fahrenheit_t >;
            map[celsius][kelvin] = cast< celsius_t, kelvin_t >;
            map[celsius][fahrenheit] = cast< celsius_t, fahrenheit_t >;
            map[fahrenheit][kelvin] = cast< fahrenheit_t, kelvin_t >;
            map[fahrenheit][celsius] = cast< fahrenheit_t, celsius_t >;
            map[feet][nautical_miles] = cast< imperial_us_length_t, nautical_mile_t >;
            map[feet][statute_miles] = cast< imperial_us_length_t, statute_mile_t >;
            map[feet][metres] = cast< imperial_us_length_t, length_t >;
            map[nautical_miles][feet] = cast< nautical_mile_t, imperial_us_length_t >;
            map[nautical_miles][statute_miles] = cast< nautical_mile_t, statute_mile_t >;
            map[nautical_miles][metres] = cast< nautical_mile_t, length_t >;
            map[statute_miles][feet] = cast< statute_mile_t, imperial_us_length_t >;
            map[statute_miles][nautical_miles] = cast< statute_mile_t, nautical_mile_t >;
            map[statute_miles][metres] = cast< statute_mile_t, length_t >;
            map[metres][feet] = cast< length_t, imperial_us_length_t >;
            map[metres][nautical_miles] = cast< length_t, nautical_mile_t >;
            map[metres][statute_miles] = cast< length_t, statute_mile_t >;
            map[hours][minutes] = boost::bind( &scale, boost::placeholders::_1, 60.0 );
            map[hours][seconds] = boost::bind( &scale, boost::placeholders::_1, 3600.0 );
            map[minutes][seconds] = boost::bind( &scale, boost::placeholders::_1, 60.0 );
            map[minutes][hours] = boost::bind( &scale, boost::placeholders::_1, 1.0 / 60.0 );
            map[seconds][hours] = boost::bind( &scale, boost::placeholders::_1, 1.0 / 3600.0 );
            map[seconds][minutes] = boost::bind( &scale, boost::placeholders::_1, 1.0 / 60.0 );
            map[percent][fraction] = boost::bind( &scale, boost::placeholders::_1, 0.01 );
            map[fraction][percent] = boost::bind( &scale, boost::placeholders::_1, 100.0 );
        }
        return map[from][to];
    }
    
    /// Test if the conversion between two measurement units is supported.
    bool can_convert( const et from, const et to ) { return NULL != cast_lookup(from, to); }
}

static void bash_completion( unsigned const ac, char const * const * av )
{
    static char const * const arguments = "--from --to --scale";
    std::cout << arguments;
    for( unsigned i = 0; i < units::count; ++i )
        std::cout << ' ' << units::name(units::et(i));
    std::cout << std::endl;
    exit( 0 );
}

/// Support reading the data from a file.
/// A type to handle a pair of fields where one is named units and the other
/// value, such that units contains the measurement standard represented by
/// the value. For example 1,feet or on another row 0.3048,metres
struct item_t
{
    double value;
    std::string units;
    item_t() : value( 0 ) {}
};

struct input_t
{
    std::vector< item_t > values;    
};

namespace comma { namespace visiting {

template <> struct traits< item_t >
{
    template < typename K, typename V > static void visit( const K&, const item_t& p, V& v )
    {
        v.apply( "value", p.value );
        v.apply( "units", p.units );
    }

    template < typename K, typename V > static void visit( const K&, item_t& p, V& v )
    {
        v.apply( "value", p.value );
        v.apply( "units", p.units );
    }
};

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
static boost::unordered_map< std::string, unsigned > input_fields;

static std::string init_input_field( const std::string& v )
{
    const std::string stripped( comma::strip( v, ' ' ) );
    if( stripped.empty() ) { return std::string(); }
    
    const size_t pos = stripped.rfind( '/' );
    std::string head, tail;
    if ( std::string::npos == pos ) // just a
    {
        head = stripped;
        tail = "value";
    }
    else
    {
        head = stripped.substr(0, pos);
        tail = stripped.substr(pos + 1, std::string::npos);
        if ( "units" != tail && "value" != tail )
        {
            head = stripped;
            tail = "value";
        }
    }
    
    unsigned idx = input_fields.size();
    if ( input_fields.cend() == input_fields.find( head ) ) { input_fields[head] = idx; }
    else { idx = input_fields.at( head ); }
    
    return "values[" + boost::lexical_cast< std::string >( idx ) + "]/" + tail;
}

static void init_input()
{
    std::string fields;
    std::string comma;
    const std::vector< std::string >& v = comma::split( csv.fields, ',' );
    for( unsigned int i = 0; i < v.size(); ++i )
    {
        fields += comma;
        comma = ",";
        fields += init_input_field( v[i] );
    }
    csv.fields = fields;
    csv.full_xpath = true;
    input.values.resize( input_fields.size() ); //input.values.resize( size );
}

static int scale_and_offset( double factor, double offset )
{
    comma::csv::input_stream< input_t > istream( std::cin, csv, input );
    comma::csv::output_stream< input_t > ostream( std::cout, csv, input );
    while( istream.ready() || ( std::cin.good() && !std::cin.eof() ) )
    {
        const input_t* p = istream.read();
        if( !p ) { break; }
        input_t output = *p;
        for( unsigned int i = 0; i < output.values.size(); output.values[i].value = output.values[i].value * factor + offset, ++i );
        ostream.write( output, istream );
    }
    return 0;
}

static int run( const units::et from, const units::et to )
{
    comma::csv::input_stream< input_t > istream( std::cin, csv, input );
    comma::csv::output_stream< input_t > ostream( std::cout, csv, input );

    units::cast_function const default_cast_function = units::cast_lookup( from, to );
    if (NULL == default_cast_function) { COMMA_THROW( comma::exception, "unsupported default conversion from " << debug_name(from) << " to " << debug_name(to) ); }
    
    unsigned line = 0;
    while( istream.ready() || ( std::cin.good() && !std::cin.eof() ) )
    {
        const input_t* p = istream.read();
        if( !p ) { break; }
        input_t output = *p;
        for( unsigned int i = 0; i < output.values.size(); ++i )
        {
            units::cast_function field_cast_function = default_cast_function;
            if ( ! output.values[i].units.empty() )
            {
                units::et field_from = units::value( output.values[i].units );
                field_cast_function = units::cast_lookup( field_from, to );
                if( NULL == field_cast_function ) { COMMA_THROW( comma::exception, "on line " << line << " unsupported conversion from " << debug_name(field_from) << " to " << debug_name(to) ); }
            }
            output.values[i].value = field_cast_function( output.values[i].value );
            output.values[i].units = units::name( to );
        }
        ostream.write( output, istream );
        ++line;
    }
    return 0;
}

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av );
        if( options.exists( "--bash-completion" ) ) bash_completion( ac, av );
        
        if( options.exists( "--help,-h" ) ) usage();
        verbose = options.exists( "--verbose,-v" );
        csv = comma::csv::options( options );
        if( csv.fields.empty() ) { csv.fields="a"; }
        init_input();
        boost::optional< double > scale_factor = options.optional< double >( "--scale" );
        boost::optional< double > offset = options.optional< double >( "--offset" );
        if( scale_factor || offset ) { return scale_and_offset( scale_factor ? *scale_factor : 1, offset ? *offset : 0 ); }
        units::et from = units::metres; // quick and dirty: to avoid compilation warning
        units::et to = units::metres; // quick and dirty: to avoid compilation warning
        if( csv.fields.find( "/units" ) == std::string::npos )
        {
            if( !options.exists( "--from" ) && !options.exists( "--to" ) ) { std::cerr << "csv-units: got neither --from nor --to" << std::endl; return 1; }
            if( options.exists( "--from" ) ) { from = units::value( options.value< std::string >( "--from" ) ); }
            if( options.exists( "--to" ) ) { to = units::value( options.value< std::string >( "--to" ) ); }
            if( !options.exists( "--from" ) ) { from = units::metric[to]; }
            if( !options.exists( "--to" ) ) { to = units::metric[from]; }
        }
        else
        {
            to = units::value( options.value< std::string >( "--to" ) );
            from = !options.exists( "--from" ) ? to : units::value( options.value< std::string >( "--from" ) );
        }
        if( !units::can_convert( from, to ) ) { std::cerr << "csv-units: don't know how to convert " << units::name(from) << " to " << units::name(to) << std::endl; return 1; }
        return run( from, to );
    }
    catch( std::exception& ex ) { std::cerr << "csv-units: caught: " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << "csv-units: unknown exception" << std::endl; }
    return 1;
}

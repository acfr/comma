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

#include "../string/split.h"
#include "../application/command_line_options.h"
#include "../base/exception.h"
#include <sstream>
#include <set>
#include <boost/bind/bind.hpp>
#include <boost/config/warning_disable.hpp>
#include <boost/optional.hpp>
#include <regex>
#include <boost/spirit/include/qi.hpp>
#include <boost/unordered_set.hpp>

#include <algorithm>

namespace comma {

command_line_options::command_line_options( int argc, char ** argv, boost::function< void( bool ) > usage )
{
    argv_.resize( argc );
    for( int i = 0; i < argc; ++i ) { argv_[i] = argv[i]; }
    fill_map_( argv_ );
    bool v=exists("--verbose,-v");
    comma::verbose.init(v, argv[0]);
    if( usage && exists( "--help,-h" ) ) { usage( v ); exit( 1 ); }
}

command_line_options::command_line_options( const std::vector< std::string >& argv, boost::function< void( bool ) > usage )
    : argv_( argv )
{
    fill_map_( argv_ );
    bool v=exists("--verbose,-v");
    comma::verbose.init(v, argv[0]);
    if( usage && exists( "--help,-h" ) ) { usage( v ); exit( 1 ); }
}

std::string command_line_options::escaped( const std::string& s ) // quick and dirty
{
    static const std::regex r( "\\\"" );
    //return s.find_first_of( "; \t\n&<>|$#*?()[]{}\'\"" ) == std::string::npos ? s : std::regex_replace( s, r, "\\\\\"" );
    return std::regex_replace( s, r, "\\\\\"" );
}

std::string command_line_options::string() const
{
    std::ostringstream out;
    std::string space;
    for( std::size_t i = 0; i < argv_.size(); ++i )
    {
        out << space << '"' << escaped( argv_[i] ) << '"';
        space = " ";
    }
    return out.str();
}

command_line_options::command_line_options( const command_line_options& rhs ) { operator=( rhs ); }

const std::vector< std::string >& command_line_options::argv() const { return argv_; }

bool command_line_options::exists( const std::string& name ) const
{
    std::vector< std::string > names = comma::split( name, ',' );
    for( std::size_t i = 0; i < names.size(); ++i )
    {
        if( map_.find( names[i] ) != map_.end() ) { return true; }
    }
    return false;
}

std::vector< std::string > command_line_options::unnamed( const std::string& valueless_options, const std::string& options_with_values ) const
{
    std::vector< std::string > valueless = split( valueless_options, ',' );
    std::vector< std::string > valued = split( options_with_values, ',' );
    std::vector< std::string > w;
    for( unsigned int i = 1; i < argv_.size(); ++i )
    {
        bool is_valueless = false;
        for( unsigned int k = 0; !is_valueless && k < valueless.size(); ++k ) { is_valueless = std::regex_match( argv_[i], std::regex( valueless[k] ) ); }
        if( is_valueless ) { continue; }
        bool is_valued = false;
        bool has_equal_sign = false;
        for( unsigned int j = 0; !is_valued && j < valued.size(); ++j )
        {
            has_equal_sign = std::regex_match( argv_[i], std::regex( valued[j] + "=.*" ) );
            is_valued = std::regex_match( argv_[i], std::regex( valued[j] ) ) || has_equal_sign;
        }
        if( is_valued ) { if( !has_equal_sign ) { ++i; } continue; }
        w.push_back( argv_[i] );
    }
    return w;
}

std::vector< std::string > command_line_options::names() const { return names_; }

void command_line_options::fill_map_( const std::vector< std::string >& v )
{
    for( std::size_t i = 1; i < v.size(); ++i )
    {
        if( v[i].length() < 2 || v[i].at( 0 ) != '-') { continue; }
        std::string name;
        boost::optional< std::string > value;
        std::size_t equal = v[i].find_first_of( '=' );
        if( equal == std::string::npos )
        {
            name = v[i];
            if( ( i + 1 ) < v.size() ) { value = v[ i + 1 ]; }
        }
        else
        {
            name = v[i].substr( 0, equal );
            if( ( equal + 1 ) < v[i].length() ) { value = v[i].substr( equal + 1 ); }
        }
//         if( v[i].at( 1 ) == '-' )
//         {
//             if( v[i].length() < 3 ) { continue; }
//             if( equal == std::string::npos )
//             {
//                 name = v[i];
//                 // check if the option has a value, i.e. not starting with '-' unless it is a negative number
//                 if( ( i + 1 < v.size() ) && ( ( v[i+1][0] != '-' ) || isdigit( v[i+1][1] ) ) ) { value = v[ i + 1 ]; }
//             }
//             else
//             {
//                 name = v[i].substr( 0, equal );
//                 value = v[i].substr( equal + 1 );
//             }
//         }
//         else
//         {
//             name = v[i];
//             if( i + 1 < v.size() ) { value = v[ i + 1 ]; }
//         }
        std::vector< std::string >& values = map_[name];
        if( value ) { values.push_back( *value ); }
        if( !name.empty() ) { names_.push_back( name ); }
    }
}

void command_line_options::assert_mutually_exclusive( const std::string& names ) const
{
    const std::vector< std::string >& v = comma::split( names, ',' );
    std::size_t count = 0;
    for( std::size_t i = 0; i < v.size(); ++i )
    {
        count += exists( v[i] );
        if( count > 1 ) { COMMA_THROW( comma::exception, "options " << names << " are mutually exclusive" ); }
    }
}

void command_line_options::assert_mutually_exclusive( const std::string& first, const std::string& second ) const
{
    const std::vector< std::string >& v = comma::split( first, ',' );
    const std::string* f = NULL;
    for( std::size_t i = 0; i < v.size(); ++i ) { if( exists( v[i] ) ) { f = &v[i]; break; } }
    if( !f ) { return; }
    const std::vector< std::string >& w = comma::split( second, ',' );
    for( std::size_t i = 0; i < w.size(); ++i ) { if( exists( w[i] ) ) { COMMA_THROW( comma::exception, "options " << *f << " and " << w[i] << " are mutually exclusive" ); } }
}

void command_line_options::assert_valid( const std::vector< description >& d, bool unknown_options_invalid )
{
    for( unsigned int i = 0; i < d.size(); ++i ) { d[i].assert_valid( *this ); }
    if( !unknown_options_invalid ) { return; }
    boost::unordered_set< std::string > s; // real quick and dirty, just to make it work
    for( unsigned int i = 0; i < d.size(); ++i ) { for( unsigned int j = 0; j < d[i].names.size(); s.insert( d[i].names[j] ), ++j ); }
    for( unsigned int i = 0; i < names_.size(); ++i ) { if( s.find( names_[i] ) == s.end() ) { COMMA_THROW( comma::exception, "unknown option " << names_[i] ); } }
}

namespace impl {
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    typedef std::string::const_iterator iterator;
    typedef comma::command_line_options::description description_t;

    static void set_( std::string& s, const std::string& t ) { s = t; }
    static void push_back_( std::vector< std::string >& v, const std::string& t ) { v.push_back( t ); }
    static void got_value( description_t& d, const std::string& ) { d.has_value = true; d.is_optional = false; }
    static void got_optional_value( description_t& d, const std::string& ) { d.has_value = true; d.is_optional = true; }
    static void got_default_value( description_t& d, const std::string& s ) { if( d.has_value ) { d.default_value = s; d.is_optional = true; } }

    comma::command_line_options::description from_string_impl_( const std::string& s )
    {
        typedef qi::rule< iterator, std::string(), ascii::space_type > rule;
        rule string = qi::lexeme[ +( boost::spirit::qi::alpha | boost::spirit::qi::digit | ascii::char_( '-' ) | ascii::char_( '_' ) ) ];
        //qi::rule< iterator, std::string(), ascii::space_type > long_name = ascii::char_( '-' ) >> ascii::char_( '-' ) >> string;
        rule name = ascii::char_( '-' ) >> string;
        rule value = '<' >> string >> '>';
        rule optional_value = qi::lit( "[<" ) >> string >> qi::lit( ">]" );
        rule double_quoted = qi::lit( "\"" ) >> qi::no_skip[ *( ( '\\' > ascii::char_( "\"" ) ) | ~ascii::char_( "\"" ) ) ] >> qi::lit( "\"" );
        rule single_quoted = qi::lit( "'" ) >> qi::no_skip[ *( ( '\\' > ascii::char_( "\'" ) ) | ~ascii::char_( "\'" ) ) ] >> qi::lit( "'" );
        rule default_value = qi::lit( "default=" ) >> ( double_quoted | single_quoted | qi::no_skip[ *( ~ascii::space - ascii::char_( ";" ) ) ] );
        rule help = qi::no_skip[ *ascii::char_ ];

        description_t d;
        bool r = boost::spirit::qi::phrase_parse( s.begin()
                                                , s.end()
                                                ,      name[ boost::bind( push_back_, boost::ref( d.names ), boost::placeholders::_1 ) ]
                                                    >> *( ',' >> name[ boost::bind( push_back_, boost::ref( d.names ), boost::placeholders::_1 ) ] )
                                                    >> -( '=' >> ( value[ boost::bind( got_value, boost::ref( d ), boost::placeholders::_1 ) ]
                                                                | optional_value[ boost::bind( got_optional_value, boost::ref( d ), boost::placeholders::_1 ) ] ) )
                                                    >> -( ';' >> default_value[ boost::bind( got_default_value, boost::ref( d ), boost::placeholders::_1 ) ] )
                                                    >> -( ';' >> *( ascii::space ) >> help[ boost::bind( set_, boost::ref( d.help ), boost::placeholders::_1 ) ] )
                                                    >> qi::eoi
                                                , ascii::space );
        if( !r ) { COMMA_THROW( comma::exception, "invalid option description: \"" << s << "\"" ); }
        if( d.names[0].size() < 3 || d.names[0][0] != '-' || d.names[0][1] != '-' ) { COMMA_THROW( comma::exception, "expected full name (e.g. --filename), got \"" << d.names[0] << "\" in: \"" << s << "\"" ); }
        if( !d.has_value ) { d.is_optional = true; d.default_value.reset(); }
        return d;
    }
} // namespace impl {

template < typename T > std::string to_string_( const T& v ) { return boost::lexical_cast< std::string >( v ); }

std::string to_string_( bool s ) { return s ? "true" : "false"; }

void command_line_options::description::assert_valid( const command_line_options& options ) const
{
    if( !has_value ) { return; }
    if( is_optional || default_value ) { return; }
    for( unsigned int i = 0; i < names.size(); ++i ) { if( options.exists( names[i] ) ) { return; } }
    COMMA_THROW( comma::exception, "please specify " << names[0] );
}

bool command_line_options::description::valid( const command_line_options& options ) const throw()
{
    if( is_optional || default_value ) { return true; }
    try { for( unsigned int i = 0; i < names.size(); ++i ) { if( options.exists( names[i] ) ) { return true; } } } catch( ... ) {}
    return false;
}

namespace impl { command_line_options::description from_string_impl_( const std::string& s ); }

command_line_options::description command_line_options::description::from_string( const std::string& s ) { return impl::from_string_impl_( s ); } // real quick and dirty, just to get it working

std::string command_line_options::description::as_string() const
{
    std::string s = names[0];
    for( unsigned int i = 1; i < names.size(); ++i ) { s += ',' + names[i]; }
    if( has_value )
    {
        s += '=';
        s += is_optional ? "[<value>]" : "<value>";
        if( default_value ) { s += "; default=" + *default_value; }
    }
    s += "; ";
    s += help;
    return s;
}

std::string comma::command_line_options::description::usage()
{
    std::ostringstream oss;
    oss << "    option description: <name>[,<name>,<name>...][=[<value>]][; default=<value>][; help], e.g:" << std::endl;
    description has_no_value;
    has_no_value.names.push_back( "--verbose" );
    has_no_value.names.push_back( "-v" );
    has_no_value.help = "option with no value";
    description has_mandatory_value;
    has_mandatory_value.names.push_back( "--filename" );
    has_mandatory_value.names.push_back( "--file" );
    has_mandatory_value.names.push_back( "--f" );
    has_mandatory_value.has_value = true;
    has_mandatory_value.help = "mandatory option with value and no default";
    description has_optional_value;
    has_optional_value.names.push_back( "--output-file" );
    has_optional_value.names.push_back( "-o" );
    has_optional_value.has_value = true;
    has_optional_value.is_optional = true;
    has_optional_value.help = "optional option with value and no default";
    description has_default_value;
    has_default_value.names.push_back( "--threshold" );
    has_default_value.names.push_back( "-t" );
    has_default_value.has_value = true;
    has_default_value.default_value = "20";
    has_default_value.help = "option with a default value";
    oss << "        " << has_no_value.as_string() << std::endl;
    oss << "        " << has_mandatory_value.as_string() << std::endl;
    oss << "        " << has_optional_value.as_string() << std::endl;
    oss << "        " << has_default_value.as_string() << std::endl;
    return oss.str();
}

} // namespace comma {

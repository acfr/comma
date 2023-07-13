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

#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <regex>
#include <boost/version.hpp>
#include "../../base/exception.h"
#include "../../application/contact_info.h"
#include "../../application/command_line_options.h"
#include "../../name_value/ptree.h"
#include "../../name_value/serialize.h"
#include "../../xpath/xpath.h"

static void usage( bool verbose = false )
{
    std::cerr << std::endl;
    std::cerr << "take json, xml, or path-value formatted data on stdin and output in path-value or another format on stdout" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat data.xml | name-value-convert [<options>]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "data options" << std::endl;
    std::cerr << "    --from <format>: input format; if this options is omitted, input format will be guessed (only for json, xml, and path-value)" << std::endl;
    std::cerr << "    --to <format>: output format; default path-value" << std::endl;
    std::cerr << std::endl;
    std::cerr << "formats" << std::endl;
    std::cerr << "    info: info data (see boost::property_tree)" << std::endl;
    std::cerr << "    ini: ini data" << std::endl;
    std::cerr << "    json: json data" << std::endl;
    std::cerr << "    xml: xml data" << std::endl;
    std::cerr << "    path-value: path=value-style data; e.g. x/a=1,x/b=2,y=3" << std::endl;
    std::cerr << std::endl;
    std::cerr << "name/path-value options:" << std::endl;
    std::cerr << "    --equal-sign,-e=<equal sign>: default '='" << std::endl;
    std::cerr << "    --delimiter,-d=<delimiter>: default ','" << std::endl;
    std::cerr << "    --no-brackets: show indices as path elements e.g. y/0/x/z/1=\"a\"" << std::endl;
    std::cerr << "          by default array items will be shown with index e.g. y[0]/x/z[1]=\"a\"" << std::endl;
    std::cerr << std::endl;
    std::cerr << "path-value options:" << std::endl;
    std::cerr << "    --take-last: if paths are repeated, take last path=value" << std::endl;
    std::cerr << "    --verify-unique,--unique-input: ensure that all input paths are unique (takes precedence over --take-last)" << std::endl;
    std::cerr << "    --unquote-numbers,--unquote: unquote the numbers and booleans" << std::endl;
    std::cerr << std::endl;
    std::cerr <<      "warning: if paths are repeated, output value selected from these inputs in not deterministic" << std::endl;
    std::cerr << std::endl;
    std::cerr << "json options" << std::endl;
    std::cerr << "    --minify: if present, output minified json" << std::endl;
    std::cerr << std::endl;
    std::cerr << "xml options" << std::endl;
    std::cerr << "    --indented: if present, output indented xml" << std::endl;
    std::cerr << "    --indent=<indent>: if present, output indented xml; default: 4" << std::endl;
    std::cerr << std::endl;
    std::cerr << "data flow options:" << std::endl;
    std::cerr << "    --linewise,-l: if present, treat each input line as a record" << std::endl;
    std::cerr << "                   if absent, treat all of the input as one record" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( 1 );
}

static comma::property_tree::xml_writer_settings_t xml_writer_settings;

static char equal_sign;
static char path_value_delimiter;
static bool linewise;
static bool minify_json;
static bool unquote_numbers;
typedef comma::property_tree::path_mode path_mode;
static path_mode indices_mode = comma::property_tree::disabled;
static bool use_index = true;
static comma::property_tree::path_value::check_repeated_paths check_type( comma::property_tree::path_value::no_check );

enum Types { ini, info, json, xml, path_value, void_t };

template < Types Type > struct traits {};

template <> struct traits< void_t >
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree ) { comma::property_tree::from_unknown( is, ptree, check_type, equal_sign, path_value_delimiter, use_index  ); }
};

template <> struct traits< ini >
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree ) { boost::property_tree::read_ini( is, ptree ); }
    static void output( std::ostream& os, const boost::property_tree::ptree& ptree, const path_mode ) { boost::property_tree::write_ini( os, ptree ); }
};

template <> struct traits< info >
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree ) { boost::property_tree::read_info( is, ptree ); }
    static void output( std::ostream& os, const boost::property_tree::ptree& ptree, const path_mode ) { boost::property_tree::write_info( os, ptree ); }
};

template <> struct traits< json >
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree ) { boost::property_tree::read_json( is, ptree ); }
    static void output( std::ostream& os, const boost::property_tree::ptree& ptree, const path_mode ) { comma::name_value::impl::write_json( os, ptree, !minify_json ); }
};

template <> struct traits< xml >
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree ) 
    { 
        comma::property_tree::read_xml( is, ptree ); 
    }
    static void output( std::ostream& os, const boost::property_tree::ptree& ptree, const path_mode )
    {
        comma::property_tree::write_xml( os, ptree, xml_writer_settings);
    }
};

template <> struct traits< path_value > // quick and dirty
{
    static void input( std::istream& is, boost::property_tree::ptree& ptree )
    {
        if( !linewise ) { comma::property_tree::from_path_value( is, ptree, check_type, equal_sign, path_value_delimiter, use_index ); return; }
        std::string line;
        std::getline( is, line );
        ptree = comma::property_tree::from_path_value_string( line, equal_sign, path_value_delimiter, check_type, use_index );
    }
    static void output( std::ostream& os, const boost::property_tree::ptree& ptree, const path_mode mode )
    {
        comma::property_tree::to_path_value( os, ptree, mode, equal_sign, path_value_delimiter, comma::xpath(), unquote_numbers );
        if( path_value_delimiter == '\n' ) { os << path_value_delimiter; }
    }
};

void ( * input )( std::istream& is, boost::property_tree::ptree& ptree );
void ( * output )( std::ostream& is, const boost::property_tree::ptree& ptree, const path_mode );

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av, usage );
        boost::optional< std::string > from = options.optional< std::string >( "--from" );
        std::string to = options.value< std::string >( "--to", "path-value" );
        equal_sign = options.value( "--equal-sign,-e", '=' );
        linewise = options.exists( "--linewise,-l" );
        minify_json = options.exists( "--minify" );
        unquote_numbers = options.exists( "--unquote-numbers,--unquote" );
        if ( options.exists( "--take-last" ) ) check_type = comma::property_tree::path_value::take_last;
        if ( options.exists( "--verify-unique,--unique-input" ) ) check_type = comma::property_tree::path_value::unique_input;
        xml_writer_settings.indent_count = options.value( "--indent", options.exists( "--indented" ) ? 4 : 0 );
        boost::optional< char > delimiter = options.optional< char >( "--delimiter,-d" );
        path_value_delimiter = delimiter ? *delimiter : ( linewise ? ',' : '\n' );
        if( from )
        {
            if( *from == "ini" ) { input = &traits< ini >::input; }
            else if( *from == "info" ) { input = &traits< info >::input; }
            else if( *from == "json" ) { input = &traits< json >::input; }
            else if( *from == "xml" ) { input = &traits< xml >::input; }
            else if( *from == "path-value" ) { input = &traits< path_value >::input; }
            else { std::cerr << "name-value-convert: expected --from format to be ini, info, json, xml, or path-value, got " << *from << std::endl; return 1; }
        }
        else
        {
            if( linewise ) {  std::cerr << "name-value-convert: if --linewise is present, --from must be given" << std::endl; return 1; }
            input = &traits< void_t >::input;
        }
        if( to == "ini" ) { output = &traits< ini >::output; }
        else if( to == "info" ) { output = &traits< info >::output; }
        else if( to == "json" ) { output = &traits< json >::output; }
        else if( to == "xml" ) { output = &traits< xml >::output; }
        else { output = &traits< path_value >::output; }
        if( use_index )
        {
            if( options.exists( "--no-brackets" ) ) { indices_mode = comma::property_tree::without_brackets; }
            else { indices_mode = comma::property_tree::with_brackets; }
        }
        if( linewise )
        {
            while( std::cout.good() )
            {
                std::string line;
                std::getline( std::cin, line );
                if( !std::cin.good() || std::cin.eof() ) { break; }
                std::istringstream iss( line );
                boost::property_tree::ptree ptree;
                input( iss, ptree );
                std::ostringstream oss;
                output( oss, ptree,  indices_mode );
                std::string s = oss.str();
                if( s.empty() ) { continue; }
                bool escaped = false;
                bool quoted = false;
                for( std::size_t i = 0; i < s.size(); ++i ) // quick and dirty
                {
                    if( escaped ) { escaped = false; continue; }
                    switch( s[i] )
                    {
                        case '\r': if( !quoted ) { s[i] = ' '; } break;
                        case '\\': escaped = true; break;
                        case '"' : quoted = !quoted; break;
                        case '\n': if( !quoted ) { s[i] = ' '; } break;
                    }

                }
                std::cout << s << std::endl;
            }
        }
        else
        {
            boost::property_tree::ptree ptree;
            input( std::cin, ptree );
            output( std::cout, ptree, indices_mode );
        }
        return 0;
    }
    catch( boost::property_tree::ptree_bad_data& ex )
    {
        std::cerr << "name-value-convert: bad data: " << ex.what() << std::endl;
    }
    catch( boost::property_tree::ptree_bad_path& ex )
    {
        std::cerr << "name-value-convert: bad path: " << ex.what() << std::endl;
    }
    catch( boost::property_tree::ptree_error& ex )
    {
        std::regex e( "<unspecified file>" );
        std::cerr << "name-value-convert: parsing error: " << std::regex_replace( std::string( ex.what() ), e, "line" ) << std::endl;
    }
    catch( std::exception& ex )
    {
        std::cerr << "name-value-convert: " << ex.what() << std::endl;
    }
    catch( ... )
    {
        std::cerr << "name-value-convert: unknown exception" << std::endl;
    }
    return 1;
}

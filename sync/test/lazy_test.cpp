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

#include <gtest/gtest.h>
#include <boost/bind/bind.hpp>
#include "../lazy.h"

namespace comma { namespace sync { namespace test {

static bool constructed;
    
struct tested
{
    tested() : a( 888 ) { constructed = true; }
    tested( unsigned int a, const std::string& name ) : a( a ), name( name ) { constructed = true; }
    unsigned int a;
    std::string name;
    
    static tested make( unsigned int a, const std::string& name ) { return tested( a, name ); }
};
    
TEST( lazy, default_constructor )
{
    {
        constructed = false;
        comma::lazy< tested > e;
        EXPECT_FALSE( constructed );
        tested f = e;
        EXPECT_EQ( 888, f.a );
        EXPECT_TRUE( constructed );
    }
    {
        constructed = false;
        comma::lazy< tested > e;
        EXPECT_FALSE( constructed );
        EXPECT_EQ( 888, e->a );
        EXPECT_EQ( 888, ( *e ).a );
        EXPECT_TRUE( constructed );
    }
}

TEST( lazy, nondefault_constructor )
{
    {
        constructed = false;
        comma::lazy< tested > e( boost::bind( &tested::make, 555, "tested" ) );
        EXPECT_FALSE( constructed );
        tested f = e;
        EXPECT_EQ( 555, f.a );
        EXPECT_TRUE( constructed );
    }
    {
        constructed = false;
        comma::lazy< tested > e( boost::bind( &tested::make, 555, "tested" ) );
        EXPECT_FALSE( constructed );
        EXPECT_EQ( 555, e->a );
        EXPECT_EQ( 555, ( *e ).a );
        EXPECT_TRUE( constructed );
    }
}

comma::lazy< tested > make_tested()
{
    static std::string name = "local"; // if not static, it will go out of scope
    return comma::lazy< tested >( boost::bind( &tested::make, 444, boost::cref( name ) ) );
}

TEST( lazy, functions )
{
    {
        constructed = false;
        comma::lazy< tested > e = make_tested();
        EXPECT_FALSE( constructed );
        EXPECT_EQ( "local", e->name );
        EXPECT_TRUE( constructed );
    }
}

} } } // namespace comma { namespace sync { namespace test {

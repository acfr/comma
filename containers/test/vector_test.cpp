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
// 3. All advertising materials mentioning features or use of this software
//    must display the following acknowledgement:
//    This product includes software developed by the The University of Sydney.
// 4. Neither the name of the The University of Sydney nor the
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
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <comma/containers/vector.h>

namespace comma {

TEST( regular_vector, usage )
{
    comma::regular_vector< double, int > v( 1.2, 0.5, 10 );
    v( 2.1 ) = 5;
    EXPECT_EQ( 5, v[ v.index( 2.1 ) ] );
}

TEST( regular_vector, index )
{
    {
        comma::regular_vector< double, int > v( 1.2, 0.5, 10 );
        EXPECT_EQ( -1, v.index( 1 ) );
        EXPECT_EQ( 0, v.index( 1.2 ) );
        EXPECT_EQ( 0, v.index( 1.3 ) );
        EXPECT_EQ( 0, v.index( 1.4999 ) );
        EXPECT_EQ( 2, v.index( 2.2 ) );
        EXPECT_EQ( 2, v.index( 2.3 ) );
        EXPECT_EQ( 2, v.index( 2.4999 ) );
    }
    {
        comma::regular_vector< double, int > v( -1.0, 0.5, 10 );
        EXPECT_EQ( -2, v.index( -1.5000001 ) );
        EXPECT_EQ( -1, v.index( -1.5 ) );
        EXPECT_EQ( -1, v.index( -1.4999999 ) );
        EXPECT_EQ( -1, v.index( -1.0000001 ) );
        EXPECT_EQ( 0, v.index( -1 ) );
        EXPECT_EQ( 0, v.index( -0.9999999 ) );
        EXPECT_EQ( 1, v.index( -0.0000001 ) );
        EXPECT_EQ( 2, v.index( 0 ) );
        EXPECT_EQ( 2, v.index( 0.0000001 ) );
    }
}

} // namespace comma {

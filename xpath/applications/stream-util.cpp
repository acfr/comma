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


/// @author m-hounsell-acfr 

#include <cassert>

#include <iostream>
#include <cstdio> // for EOF

#include <algorithm>
#include <iterator>

#include <comma/io/stream-util.h>

#include <comma/xpath/xpath.h>

comma::io::newline_t comma::io::newline;

std::istream &
comma::io::ignore_until_newline(std::istream & is)
{
    for (;;)
    {
        char const c = is.get();
        if (! is || '\n' == c) return is;
        if ('\r' != c ) continue;

        // we got a \r so test for \n
        int next = is.peek();
        if (EOF == next) return is;
        if ('\n' == next) next = is.get();
        return is;
    }
}

std::istream &
operator >>(std::istream & is, comma::io::punct const & p)
{
    char const c = is.get();
    if (c != p._c) is.setstate(std::ios::failbit);
    return is;
}

std::istream &
operator >>(std::istream & is, comma::io::newline_t const & p)
{
    char const c = is.get();
    if (! is || '\n' == c) return is;
    if ('\r' != c )
    {
        is.setstate(std::ios::failbit);
        return is;
    }
    int next = is.peek();
    if (EOF == next) return is;
    if ('\n' == next) next = is.get();
    return is;
}
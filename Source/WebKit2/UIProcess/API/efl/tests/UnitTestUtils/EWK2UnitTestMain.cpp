/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "EWK2UnitTestBase.h"
#include "EWK2UnitTestEnvironment.h"
#include <getopt.h>
#include <gtest/gtest.h>

using namespace EWK2UnitTest;

EWK2UnitTestEnvironment* environment = 0;

static bool parseArguments(int argc, char** argv)
{
    int useX11Window = 0;

    static const option options[] = {
        {"useX11Window", no_argument, &useX11Window, 1},
        {0, 0, 0, 0}
    };

    while (getopt_long(argc, argv, "", options, 0) != -1) { }

    return useX11Window;
}

int main(int argc, char** argv)
{
    bool useX11Window = parseArguments(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);

    environment = new EWK2UnitTestEnvironment(useX11Window);
    testing::AddGlobalTestEnvironment(environment);

    return RUN_ALL_TESTS();
}

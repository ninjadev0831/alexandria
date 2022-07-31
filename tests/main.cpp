/*
 * MIT License
 *
 * Alexandria.org
 *
 * Copyright (c) 2021 Josef Cullhed, <info@alexandria.org>, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define BOOST_TEST_MODULE "Unit tests for alexandria.org"

#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>

#include "config.h"
#include "logger/logger.h"
#include "worker/worker.h"

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <streambuf>
#include <math.h>
#include <vector>
#include <set>
#include <map>

using std::string;
using std::vector;
using std::ifstream;
using std::stringstream;
using std::set;
using std::map;
using std::pair;

void run_before() {
	config::read_config("../tests/test_config.conf");
	logger::start_logger_thread();
	worker::start_urlstore_server();
}

void run_after() {
	logger::join_logger_thread();
}

int BOOST_TEST_CALL_DECL
main(int argc, char* argv[]) {

	run_before();

    int ret = ::boost::unit_test::unit_test_main(&init_unit_test, argc, argv);

	run_after();

	return ret;
}


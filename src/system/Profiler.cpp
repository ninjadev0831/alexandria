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

#include "Profiler.h"
#include <vector>

namespace Profiler {

	instance::instance(const string &name) :
		m_name(name)
	{
		m_start_time = std::chrono::high_resolution_clock::now();
	}

	instance::instance() :
		m_name("unnamed profile")
	{
		m_start_time = std::chrono::high_resolution_clock::now();
	}

	instance::~instance() {
		if (!m_has_stopped) {
			stop();
		}
	}

	void instance::enable() {
		m_enabled = true;
	}

	double instance::get() const {
		if (!m_enabled) return 0;
		auto timer_elapsed = chrono::high_resolution_clock::now() - m_start_time;
		auto microseconds = chrono::duration_cast<std::chrono::microseconds>(timer_elapsed).count();

		return (double)microseconds/1000;
	}

	double instance::get_micro() const {
		if (!m_enabled) return 0;
		auto timer_elapsed = chrono::high_resolution_clock::now() - m_start_time;
		auto microseconds = chrono::duration_cast<std::chrono::microseconds>(timer_elapsed).count();

		return (double)microseconds;
	}

	void instance::stop() {
		if (!m_enabled) return;
		m_has_stopped = true;
		cout << "Profiler [" << m_name << "] took " << get() << "ms" << endl;
	}

	void instance::print() {
		if (!m_enabled) return;
		cout << "Profiler [" << m_name << "] took " << get() << "ms" << endl;
	}

	void print_memory_status() {
		ifstream infile("/proc/" + to_string(getpid()) + "/status");
		if (infile.is_open()) {
			string line;
			while (getline(infile, line)) {
				cout << line << endl;
			}
		}
	}

	uint64_t get_cycles() {
		#if PROFILE_CPU_CYCLES
		return __rdtsc();
		#endif
		return 0ull;
	}

	double base_performance = 1.0;
	void measure_base_performance() {
		instance p;
		for (size_t i = 0; i < 1000; i++) {
			vector<int> vec;
			for (size_t j = 0; j < 10000; j++) {
				vec.push_back(rand());
			}

			sort(vec.begin(), vec.end());
		}
		base_performance = p.get();
	}

	double get_absolute_performance(double elapsed_ms) {
		return elapsed_ms/base_performance;
	}

}


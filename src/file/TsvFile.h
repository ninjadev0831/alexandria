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

#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <vector>
#include <map>
#include <string.h>

#define TSV_FILE_DESTINATION "/mnt/0"

using namespace std;

class TsvFile {

public:

	TsvFile();
	TsvFile(const string &file_name);
	~TsvFile();

	// Returns the line with the first column equals key. Returns string::npos if not present in file.
	string find(const string &key);

	/*
		Returns the position of the FIRST line in the file with first column equals key.
		Returns string::npos if not present in file.
	*/
	size_t find_first_position(const string &key);
	
	/*
		Returns the position of the LAST line in the file with first column equals key.
		Returns string::npos if not present in file.
	*/
	size_t find_last_position(const string &key);

	/*
		Returns the position of the line AFTER the line in the file with first column equals key.
		If the key does not exist it returns the position to the line where this key would be inserted. If the
		key should be inserted to the end it returns m_file_size
	*/
	size_t find_next_position(const string &key);

	map<string, string> find_all(const set<string> &keys);

	size_t read_column_into(int column, set<string> &container);
	size_t read_column_into(int column, set<string> &container, size_t limit);
	size_t read_column_into(int column, vector<string> &container);
	size_t read_column_into(int column, vector<string> &container, size_t limit);

	size_t size() const;
	bool eof() const;
	bool is_open() const;
	string get_line();

protected:

	string m_file_name;
	string m_original_file_name;
	ifstream m_file;
	size_t m_file_size;
	bool m_is_gzipped = false;
	
	/*
		Difference is that _any returns the position where this key WOULD be if it was inserted even if it is not
		present.
	*/
	size_t binary_find_position(size_t file_size, size_t offset, const string &key);
	size_t binary_find_position_any(size_t file_size, size_t offset, const string &key);

	void set_file_name(const string &file_name);

};

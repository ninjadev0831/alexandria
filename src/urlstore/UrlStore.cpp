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

#include <thread>
#include "UrlStore.h"
#include "config.h"
#include "transfer/Transfer.h"
#include "parser/URL.h"
#include "system/Profiler.h"
#include "file/File.h"
#include "json.hpp"
#include <boost/filesystem.hpp>

using namespace std;
using json = nlohmann::ordered_json;

namespace UrlStore {

	// Internal data structure, this is what is stored in byte array together with URLs.
	struct UrlDataStore {
		size_t link_count;
		size_t http_code;
		size_t last_visited;
	};

	string data_to_str(const UrlData &data) {

		UrlDataStore data_store = {
			.link_count = data.link_count,
			.http_code = data.http_code,
			.last_visited = data.last_visited
		};

		string str;
		str.append((char *)&data_store, sizeof(data_store));

		const size_t url_len = data.url.size();
		str.append((char *)&url_len, sizeof(url_len));
		str.append(data.url.str());

		const size_t redirect_len = data.redirect.size();
		str.append((char *)&redirect_len, sizeof(redirect_len));
		str.append(data.redirect.str());

		return str;
	}

	UrlData str_to_data(const char *cstr, size_t len) {

		if (len < sizeof(UrlDataStore) + 2*sizeof(size_t)) {
			return UrlData{};
		}

		UrlDataStore data_store = *((UrlDataStore *)&cstr[0]);

		const size_t offs_url_len = sizeof(UrlDataStore);
		const size_t offs_url = offs_url_len + sizeof(size_t);
		const size_t url_len = *((size_t *)&cstr[offs_url_len]);

		const size_t offs_redirect_len = offs_url + url_len;
		if (offs_redirect_len >= len) return UrlData{};

		const size_t offs_redirect = offs_redirect_len + sizeof(size_t);
		const size_t redirect_len = *((size_t *)&cstr[offs_redirect_len]);

		UrlData url_data = {
			.link_count = data_store.link_count,
			.http_code = data_store.http_code,
			.last_visited = data_store.last_visited
		};

		if (offs_redirect + redirect_len <= len) {
			string url_str = string(&cstr[offs_url], url_len);
			url_data.url = URL(url_str);
			string redirect_str = string(&cstr[offs_redirect], *((size_t *)&cstr[offs_redirect_len]));
			url_data.redirect = URL(redirect_str);
		}

		return url_data;
	}

	UrlData str_to_data(const string &str) {
		return str_to_data(str.c_str(), str.size());
	}

	UrlStoreBatch::UrlStoreBatch()
	: m_batches(Config::url_store_shards)
	{
		
	}

	UrlStoreBatch::~UrlStoreBatch() {

	}

	void UrlStoreBatch::set(const UrlData &data) {
		const size_t shard = data.url.hash() % Config::url_store_shards;
		m_batches[shard].Put(data.url.key(), data_to_str(data));
	}

	UrlStore::UrlStore() : UrlStore("/mnt") {
	}

	UrlStore::UrlStore(const string &path_prefix)  {
		for (size_t i = 0; i < Config::url_store_shards; i++) {
			boost::filesystem::create_directories(path_prefix + "/" + to_string(i % 8) + "/url_store_" + to_string(i));
			m_shards.push_back(new KeyValueStore(path_prefix + "/" + to_string(i % 8) + "/url_store_" + to_string(i)));
		}
	}

	UrlStore::~UrlStore() {
		for (KeyValueStore *shard : m_shards) {
			delete shard;
		}
	}

	void UrlStore::set(const UrlData &data) {
		const size_t shard = data.url.hash() % Config::url_store_shards;
		m_shards[shard]->set(data.url.key(), data_to_str(data));
	}

	UrlData UrlStore::get(const URL &url) {
		const size_t shard = url.hash() % Config::url_store_shards;
		string value = m_shards[shard]->get(url.key());
		if (value.size()) return str_to_data(value);
		return UrlData{};
	}

	void UrlStore::write_batch(UrlStoreBatch &batch) {
		for (size_t shard = 0; shard < Config::url_store_shards; shard++) {
			m_shards[shard]->db()->Write(leveldb::WriteOptions(), &batch.m_batches[shard]);
		}
	}

	bool UrlStore::has_pending_insert() {
		return m_pending_inserts.size() > 0;
	}

	string UrlStore::next_pending_insert() {
		string file = m_pending_inserts.front();
		m_pending_inserts.pop_front();
		return file;
	}

	void UrlStore::add_pending_insert(const string &file) {
		m_pending_inserts.push_back(file);
	}

	void UrlStore::compact_all_if_full() {
		for (KeyValueStore *shard : m_shards) {
			if (shard->is_full()) {
				compact_all();
				break;
			}
		}
	}

	void UrlStore::compact_all() {
		vector<thread> threads;
		for (KeyValueStore *shard : m_shards) {
			threads.emplace_back(thread([shard]() {
				shard->compact();
			}));
		}
		for (thread &th : threads) {
			th.join();
		}
	}

	void print_binary_url_data(const UrlData &data, ostream &stream) {
		stream << data_to_str(data);
	}

	json data_to_json(const UrlData &data) {
		json message;
		message["url"] = data.url.str();
		message["redirect"] = data.redirect.str();
		message["link_count"] = data.link_count;
		message["http_code"] = data.http_code;
		message["last_visited"] = data.last_visited;

		return message;
	}

	void print_url_data(const UrlData &data, ostream &stream) {
		stream << data_to_json(data).dump(4);
	}

	void print_url_data(const UrlData &data) {
		return print_url_data(data, cout);
	}

	void apply_update(UrlData &dest, const UrlData &src, size_t update_bitmask) {
		if (update_bitmask & update_redirect) dest.redirect = src.redirect;
		if (update_bitmask & update_link_count) dest.link_count = src.link_count;
		if (update_bitmask & update_http_code) dest.http_code = src.http_code;
		if (update_bitmask & update_last_visited) dest.last_visited = src.last_visited;
	}

	void handle_put_request(UrlStore &store, const std::string &write_data, std::stringstream &response_stream) {

		Profiler::instance prof1("parse and put in batch");

		const char *cstr = write_data.c_str();
		const size_t len = write_data.size();
		if (len < 2*sizeof(size_t)) return;
		const size_t deferr_bitmask = *((size_t *)&cstr[0]);

		if (deferr_bitmask) {
			// Only save put data to disk. Then a background job will consume the files.
			store_write_data(store, write_data);
		} else {
			const size_t update_bitmask = *((size_t *)&cstr[sizeof(size_t)]);

			size_t iter = sizeof(size_t) * 2;
			UrlStoreBatch batch;
			while (iter < len) {
				size_t data_len = *((size_t *)&cstr[iter]);
				iter += sizeof(size_t);

				if (data_len + iter > len) break;

				UrlData data = str_to_data(&cstr[iter], data_len);
				if (update_bitmask) {
					UrlData to_update = store.get(data.url);
					apply_update(to_update, data, update_bitmask);
					batch.set(to_update);
				} else {
					batch.set(data);
				}

				iter += data_len;
			}
			prof1.stop();

			Profiler::instance prof2("leveldb Write");
			store.write_batch(batch);
		}
	}

	void handle_binary_get_request(UrlStore &store, const URL &url, std::stringstream &response_stream) {
		UrlData data = store.get(url);
		print_binary_url_data(data, response_stream);
	}

	void handle_get_request(UrlStore &store, const URL &url, std::stringstream &response_stream) {
		UrlData data = store.get(url);
		print_url_data(data, response_stream);
	}

	vector<URL> post_data_to_urls(const string &post_data) {
		vector<URL> urls;
		stringstream ss(post_data);
		string line;
		while (getline(ss, line)) {
			urls.emplace_back(URL(line));
		}
		return urls;
	}

	void handle_binary_post_request(UrlStore &store, const string &post_data, std::stringstream &response_stream) {
		vector<URL> urls = post_data_to_urls(post_data);
		for (const URL &url : urls) {
			UrlData data = store.get(url);
			const string bin_data = data_to_str(data);
			const size_t len = bin_data.size();
			response_stream.write((char *)&len, sizeof(size_t));
			response_stream.write(bin_data.c_str(), len);
		}
	}

	void handle_post_request(UrlStore &store, const string &post_data, std::stringstream &response_stream) {
		vector<URL> urls = post_data_to_urls(post_data);
		json arr;
		for (const URL &url : urls) {
			UrlData data = store.get(url);
			json message = data_to_json(data);
			arr.push_back(message);
		}
		response_stream << arr.dump(4);
	}

	void append_data_str(const UrlData &data, string &append_to) {
		string data_str = data_to_str(data);
		size_t data_len = data_str.size();
		append_to.append((char *)&data_len, sizeof(size_t));
		append_to.append(data_str);
	}

	void append_bitmask(size_t bitmask, string &append_to) {
		append_to.append((char *)&bitmask, sizeof(size_t));
	}

	void set(const vector<UrlData> &datas) {
		update(datas, 0x0, 0x0);
	}

	void set_deferred(const vector<UrlData> &datas) {
		update(datas, 0x0, 0x1);
	}

	void set(const UrlData &data) {
		update(data, 0x0);
	}

	void update(const std::vector<UrlData> &datas, size_t update_bitmask) {
		update(datas, update_bitmask, 0x0);
	}

	void update(const std::vector<UrlData> &datas, size_t update_bitmask, size_t deferred) {
		string put_data;
		append_bitmask(deferred, put_data);
		append_bitmask(update_bitmask, put_data);
		for (const UrlData &data : datas) {
			append_data_str(data, put_data);
		}
		Transfer::put(Config::url_store_host + "/urlstore", put_data);
	}

	void update(const UrlData &data, size_t update_bitmask) {
		string put_data;
		append_bitmask(0x0, put_data); // Single updates don't need to be dereferred.
		append_bitmask(update_bitmask, put_data);
		append_data_str(data, put_data);
		Transfer::put(Config::url_store_host + "/urlstore", put_data);
	}

	int get(const URL &url, UrlData &data) {
		Transfer::Response res = Transfer::get(Config::url_store_host + "/urlstore/" + url.str(), {"Accept: application/octet-stream"});
		if (res.code == 200) {
			data = str_to_data(res.body);
			return OK;
		}
		return ERROR;
	}

	int get(const std::vector<URL> &urls, std::vector<UrlData> &data) {
		vector<string> post_urls;
		for (const URL &url : urls) {
			post_urls.push_back(url.str());
		}
		const string post_data = boost::algorithm::join(post_urls, "\n");
		Transfer::Response res = Transfer::post(Config::url_store_host + "/urlstore", post_data, {"Accept: application/octet-stream"});
		if (res.code == 200) {

			const size_t len = res.body.size();
			const char *cstr = res.body.c_str();
			size_t iter = 0;
			while (iter < len) {
				size_t data_len = *((size_t *)&cstr[iter]);
				iter += sizeof(size_t);

				if (data_len + iter > len) break;

				data.emplace_back(str_to_data(&cstr[iter], data_len));

				iter += data_len;
			}
			return OK;
		}
		return ERROR;
	}
	
	void store_write_data(UrlStore &store, const string &write_data) {
		using namespace std::chrono;
		auto now = std::chrono::system_clock::now();
		auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		auto epoch = now_ms.time_since_epoch();
		auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
		long micros = value.count();

		const string filename = Config::url_store_cache_path + "/" + to_string(micros) + "-" + to_string(rand()) + ".cache";
		ofstream outfile(filename, ios::binary | ios::trunc);
		outfile << write_data;

		outfile.close();

		store.add_pending_insert(filename);
	}

	void consume_write_data(UrlStore &store, const string &write_data) {
		Profiler::instance prof1("parse and put in batch");

		const char *cstr = write_data.c_str();
		const size_t len = write_data.size();
		if (len < 2*sizeof(size_t)) return;
		//const size_t deferr_bitmask = *((size_t *)&cstr[0]);
		const size_t update_bitmask = *((size_t *)&cstr[sizeof(size_t)]);

		size_t iter = sizeof(size_t) * 2;
		UrlStoreBatch batch;
		while (iter < len) {
			size_t data_len = *((size_t *)&cstr[iter]);
			iter += sizeof(size_t);

			if (data_len + iter > len) break;

			UrlData data = str_to_data(&cstr[iter], data_len);
			if (update_bitmask) {
				UrlData to_update = store.get(data.url);
				apply_update(to_update, data, update_bitmask);
				batch.set(to_update);
			} else {
				batch.set(data);
			}

			iter += data_len;
		}
		prof1.stop();

		Profiler::instance prof2("leveldb Write");
		store.write_batch(batch);
	}

	void run_one_inserter(UrlStore &url_store, mutex &_mutex) {
		_mutex.lock();
		if (url_store.has_pending_insert()) {
			const string filename = url_store.next_pending_insert();
			_mutex.unlock();

			ifstream infile(filename, ios::binary);
			stringstream buffer;
			buffer << infile.rdbuf();
			const string write_data = buffer.str();

			consume_write_data(url_store, write_data);

			File::delete_file(filename);

			return;
		}
		_mutex.unlock();

	}

	void run_inserter(UrlStore &url_store) {

		url_store.compact_all_if_full();

		if (url_store.has_pending_insert()) {

			const size_t num_threads = 16;
			vector<thread> threads;
			mutex _mutex;
			for (size_t i = 0; i < num_threads; i++) {
				threads.emplace_back(thread(run_one_inserter, ref(url_store), ref(_mutex)));
			}

			for (thread &th : threads) {
				th.join();
			}
		}

	}

	void testing() {
		leveldb::DB *db;
		leveldb::Options options;
		options.create_if_missing = true;
		options.write_buffer_size = 64ull * 1024ull * 1024ull;
		leveldb::Status status = leveldb::DB::Open(options, "/mnt/0/testdb0", &db);
		if (!status.ok()) {
			cout << "Could not open database: " << endl;
		}

		while (true) {
			leveldb::WriteBatch batch;
			for (size_t i = 0; i < 1000000; i++) {
				batch.Put(to_string(rand()), to_string(rand()) + to_string(rand()) + to_string(rand()) + to_string(rand()));
			}
			db->Write(leveldb::WriteOptions(), &batch);

			{
				string num_files;
				db->GetProperty("leveldb.num-files-at-level0", &num_files);
				if (num_files == "12") {
					cout << "compacting all!" << endl;
					db->CompactRange(nullptr, nullptr);
				}
			}

			cout << "leveldb.num-files-at-level: ";
			for (size_t level = 0; level < 10; level++) {
				string num_files;
				db->GetProperty("leveldb.num-files-at-level" + to_string(level), &num_files);
				cout << num_files << " ";
			}
			cout << endl;

			string stats;
			db->GetProperty("leveldb.stats", &stats);
			cout << stats << endl;
		}

		delete db;
	}
}

#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <sstream>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Dynamic/Var.h>

using String = std::string;

// The current O(N^2) ClickHouse implementation
String removeEscapedSlashes_OLD(const String & json_str)
{
    auto result = json_str;
    size_t pos = 0;
    while ((pos = result.find("\\/", pos)) != std::string::npos)
    {
        result.replace(pos, 2, "/");
        ++pos;
    }
    return result;
}

// Your new O(N) implementation
String removeEscapedSlashes_NEW(const String & json_str)
{
    size_t pos = json_str.find("\\/");
    if (pos == String::npos)
        return json_str;

    String result;
    result.reserve(json_str.size());

    size_t start = 0;
    while (pos != String::npos)
    {
        result.append(json_str, start, pos - start);
        result.push_back('/');

        start = pos + 2;
        pos = json_str.find("\\/", start);
    }
    result.append(json_str, start, String::npos);

    return result;
}

String dumpMetadataObjectToString_OLD(const Poco::JSON::Object::Ptr & metadata_object)
{
    std::ostringstream oss; // STYLE_CHECK_ALLOW_STD_STRING_STREAM
    Poco::JSON::Stringifier::stringify(metadata_object, oss);
    return removeEscapedSlashes_OLD(oss.str());
}

String dumpMetadataObjectToString_NEW(const Poco::JSON::Object::Ptr & metadata_object)
{
    std::ostringstream oss; // STYLE_CHECK_ALLOW_STD_STRING_STREAM
    Poco::JSON::Stringifier::stringify(metadata_object, oss);
    return removeEscapedSlashes_NEW(oss.str());
}

int main(int argc, char** argv) {
    std::string filename = (argc > 1) ? argv[1] : "metadata.json";
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "Could not open " << filename << std::endl;
        return 1;
    }

    auto raw_size = ifs.tellg();
    String raw_content(raw_size, '\0');
    ifs.seekg(0);
    ifs.read(&raw_content[0], raw_size);

    std::cout << "File loaded: " << (static_cast<double>(raw_size) / 1024.0 / 1024.0) << " MB" << std::endl;

    // Parse with Poco to get a "real" object
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var parsed_result = parser.parse(raw_content);
    Poco::JSON::Object::Ptr metadata_object;

    if (parsed_result.type() == typeid(Poco::JSON::Object::Ptr))
        metadata_object = parsed_result.extract<Poco::JSON::Object::Ptr>();
    else
    {
        std::cerr << "Parsed JSON is not an object" << std::endl;
        return 1;
    }

    // Stringify to get the version with escaped slashes
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(metadata_object, oss);
    String stringified_content = oss.str();
    size_t size = stringified_content.size();

    std::cout << "Stringified (POCO) size: " << (static_cast<double>(size) / 1024.0 / 1024.0) << " MB\n" << std::endl;

    // 1. Verify correctness
    std::cout << "Verifying correctness... ";
    String check_old = dumpMetadataObjectToString_OLD(metadata_object);
    String check_new = dumpMetadataObjectToString_NEW(metadata_object);
    if (check_old != check_new) {
        std::cerr << "FAIL! Outputs do not match." << std::endl;
        return 1;
    }
    std::cout << "PASS. Both functions produce identical strings.\n" << std::endl;

    volatile size_t dummy_sink = 0;

    // 2. Benchmark OLD implementation
    int old_iterations = 3;
    std::cout << "Benchmarking dumpMetadataObjectToString with OLD implementation (" << old_iterations << " iterations)..." << std::endl;
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < old_iterations; ++i) {
        dummy_sink += dumpMetadataObjectToString_OLD(metadata_object).length();
    }
    auto end_old = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_old = end_old - start_old;

    std::cout << "Average time: " << (diff_old.count() / old_iterations) * 1000 << " ms" << std::endl;
    std::cout << "Throughput: " << (static_cast<double>(size) * old_iterations / (1024*1024)) / diff_old.count() << " MB/s\n" << std::endl;

    // 3. Benchmark NEW implementation
    int new_iterations = 10;
    std::cout << "Benchmarking dumpMetadataObjectToString with NEW implementation (" << new_iterations << " iterations)..." << std::endl;
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < new_iterations; ++i) {
        dummy_sink += dumpMetadataObjectToString_NEW(metadata_object).length();
    }
    auto end_new = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_new = end_new - start_new;

    std::cout << "Average time: " << (diff_new.count() / new_iterations) * 1000 << " ms" << std::endl;
    std::cout << "Throughput: " << (static_cast<double>(size) * new_iterations / (1024*1024)) / diff_new.count() << " MB/s" << std::endl;

    return 0;
}

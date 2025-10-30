#include "json_parser.hpp"
#include "query_builder.hpp"
#include <pqxx/pqxx>
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>

struct Args {
    std::string query_file;
    std::string result_file = "results.txt";
    bool help = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args.help = true;
        } else if (arg == "--query-file") {
            if (++i >= argc) {
                throw std::runtime_error("Missing value for --query-file");
            }
            args.query_file = argv[i];
        } else if (arg == "--result-file") {
            if (++i >= argc) {
                throw std::runtime_error("Missing value for --result-file");
            }
            args.result_file = argv[i];
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (args.query_file.empty() && !args.help) {
        throw std::runtime_error("--query-file is required");
    }
    return args;  
}

void help_message() {
    std::cout << "Usage: query --query-file <q.json> [--result-file result.txt]\n";
}

int main(int argc, char* argv[]) {
    try {
        Args args = parse_args(argc, argv);
        if (args.help) {
            help_message();
            return 0;
        }

        auto [valid_region, query] = QueryParser::parse_full(args.query_file);

        pqxx::connection conn("dbname=inspection user=postgres password=1049 host=localhost port=5432");
        QueryBuilder qb(conn, valid_region);
        auto results = qb.execute(query);

        std::ofstream result_file(args.result_file);
        for (const auto& point : results) {
            result_file << std::fixed << std::setprecision(6) << point.x << " " << point.y << "\n";
        }

        std::cout << "Query executed successfully. " << results.size() << " points -> " << args.result_file << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
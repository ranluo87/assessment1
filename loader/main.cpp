#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>

#include <pqxx/pqxx> // Include the main libpqxx header

using namespace std;

struct Args {
    std::string data_directory;
    bool help = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args.help = true;
        } else if (arg == "--data-directory") {
            if (++i >= argc) {
                throw std::runtime_error("Missing value for --data-directory");
            }
            args.data_directory = argv[i];
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (args.data_directory.empty() && !args.help) {
        throw std::runtime_error("--data-directory is required");
    }
    return args;    
}

struct Region {
    double x, y;
    int category;
    long long group_id;
    long long id;
};

std::vector<Region> load_data(const std::string& dir) {
    auto real_lines = [&](const std::string& filename) -> std::vector<std::string> {
        std::ifstream file(dir + "/" + filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }
        std::vector<std::string> lines;
        std::string line;   
        while (std::getline(file, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    };

    auto points = real_lines("points.txt");
    auto categories = real_lines("categories.txt");
    auto groups = real_lines("groups.txt");

    if (points.size() != categories.size() || points.size() != groups.size()) {
        throw std::runtime_error("Data files have inconsistent number of lines");
    }

    std::vector<Region> data;
    data.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        std::istringstream ps(points[i]), rs(categories[i]), gs(groups[i]);
        Region r;
        r.id = static_cast<long long>(i);
        
        if(!(ps >> r.x >> r.y)) throw std::runtime_error("Invalid line in points.txt: " + points[i]);
        if(!(rs >> r.category)) throw std::runtime_error("Invalid category");
        if(!(gs >> r.group_id)) throw std::runtime_error("Invalid groups");

        data.push_back(r);
    }

    return data;
}

int main(int argc, char* argv[]) {
    try {
        Args args = parse_args(argc, argv);
        if (args.help) {
            std::cout << "Usage: loader --data-directory <dir>\n";
            return 0;
        }

        auto data = load_data(args.data_directory);

        pqxx::connection conn("dbname=inspection user=postgres password=1049 host=localhost port=5432");
        pqxx::work txn(conn);
        
        // Delete existing data before loading new data
        txn.exec("DELETE FROM inspection_region");
        txn.exec("DELETE FROM inspection_group");

        std::set<long long> unique_groups;
        for (const auto& region : data) {
            unique_groups.insert(region.group_id);
            txn.exec_params(
                "INSERT INTO inspection_region (id, coord_x, coord_y, category, group_id) VALUES ($1, $2, $3, $4, $5)",
                region.id, region.x, region.y, region.category, region.group_id
            );
        }

        for (const auto& group_id : unique_groups) {
            txn.exec_params("INSERT INTO inspection_group (id) VALUES ($1) ON CONFLICT DO NOTHING", group_id);
        }

        txn.commit();
        std::cout << "Data loaded successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
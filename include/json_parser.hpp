#pragma once
#include "types.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

using json = nlohmann::json;

struct CropQuery {
    Region region;
    std::optional<int> category;
    std::optional<std::vector<int64_t>> one_of_groups;
    bool proper = false;
};

struct QueryNode {
    enum class Type {Crop, And, Or} type;
    std::vector<CropQuery> crops; // valid if type == Crop
    std::vector<QueryNode> children; // valid if type == And or Or
};

class QueryParser {
public:
    static Region parse_region(const json& j);
    static CropQuery parse_crop(const json& j);
    static QueryNode parse_query(const json& j);
    static std::pair<Region, QueryNode> parse_full(const std::string& filename);
};


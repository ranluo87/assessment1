#include "json_parser.hpp"
#include <fstream>

Region QueryParser::parse_region(const json& j) {
    return {
        j["p_min"]["x"].get<double>(),
        j["p_min"]["y"].get<double>(),
        j["p_max"]["x"].get<double>(),
        j["p_max"]["y"].get<double>()
    };
}

CropQuery QueryParser::parse_crop(const json& j) {
    CropQuery q;
    q.region = parse_region(j["region"]);
    if (j.contains("category")) {
        q.category = j["category"].get<int>();
    }
    if (j.contains("one_of_groups")) {
        q.one_of_groups = j["one_of_groups"].get<std::vector<int64_t>>();
    }
    if (j.contains("proper")) {
        q.proper = j["proper"].get<bool>();
    }
    return q;
}

QueryNode QueryParser::parse_query(const json& j) {
    QueryNode node;
    if (j.contains("operator_crop")) {
        node.type = QueryNode::Type::Crop;
        node.crops.push_back(parse_crop(j["operator_crop"]));
    } else if (j.contains("operator_and")) {
        node.type = QueryNode::Type::And;
        for (const auto& child : j["operator_and"]) {
            node.children.push_back(parse_query(child));
        }
    } else if (j.contains("operator_or")) {
        node.type = QueryNode::Type::Or;
        for (const auto& child : j["operator_or"]) {
            node.children.push_back(parse_query(child));
        }
    } else {
        throw std::runtime_error("Unknown operator node in JSON");
    }
    return node;
}

std::pair<Region, QueryNode> QueryParser::parse_full(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open JSON file: " + filename);
    }
    json data = json::parse(file);

    Region valid = parse_region(data["valid_region"]);
    QueryNode query = parse_query(data["query"]);

    return {valid, query};
}
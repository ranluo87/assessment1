#pragma once
#include "types.hpp"
#include "json_parser.hpp"

#include <pqxx/pqxx>
#include <set>

class QueryBuilder {
    pqxx::connection& conn;
    const Region& valid_region;

public:
    QueryBuilder(pqxx::connection& c, const Region& vr) : conn(c), valid_region(vr) {}
    std::set<Point2D> execute(const QueryNode& node);
private:
    std::set<Point2D> exec_crop(const CropQuery& q);
    std::set<int64_t> get_proper_groups();
};
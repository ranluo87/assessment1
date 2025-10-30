#include "query_builder.hpp"
#include <sstream>
#include <algorithm>

std::set<int64_t> QueryBuilder::get_proper_groups() {
    std::set<int64_t> proper;
    std::string sql = R"(
        SELECT r1.group_id
        FROM inspection_region r1
        WHERE r1.coord_x >= $1 AND r1.coord_x <= $2
          AND r1.coord_y >= $3 AND r1.coord_y <= $4
        GROUP BY r1.group_id
        HAVING COUNT(*) = (
            SELECT COUNT(*) FROM inspection_region r2
            WHERE r2.group_id = r1.group_id
          )
    )";
    pqxx::work txn(conn);
    auto r = txn.exec_params(
        sql,
        valid_region.min_x, valid_region.max_x,
        valid_region.min_y, valid_region.max_y
    );

    for (const auto& row : r) {
        proper.insert(row[0].as<int64_t>());
    }
}

std::set<Point2D> QueryBuilder::exec_crop(const CropQuery& q) {
    std::ostringstream sql;
    sql << "SELECT coord_x, coord_y FROM inspection_region WHERE ";
    std::vector<std::string> conditions;
    std::vector<pqxx::params> params;

    // Bounding box condtions
    conditions.push_back("(coord_x >= $1 AND coord_x <= $2 AND coord_y >= $3 AND coord_y <= $4)");
    params.emplace_back(std::to_string(q.region.min_x), sizeof(double));
    params.emplace_back(std::to_string(q.region.max_x), sizeof(double));
    params.emplace_back(std::to_string(q.region.min_y), sizeof(double));
    params.emplace_back(std::to_string(q.region.max_y), sizeof(double));

    // Category condition
    if (q.category) {
        conditions.push_back("category = $" + std::to_string(params.size() + 1));
        params.emplace_back(std::to_string(int(*q.category)), sizeof(int));
    }

    // One-of-groups condition
    if (q.one_of_groups) {
        std::ostringstream in;
        for (size_t i = 0; i < q.one_of_groups->size(); ++i) {
            if (i > 0) in << ", ";
            in << "$" << (params.size() + 1);
            params.emplace_back(std::to_string((*q.one_of_groups)[i]), sizeof(int64_t));
        }
    }

    // Proper groups condition
    if (q.proper) {
        auto proper_groups = get_proper_groups();
        if (proper_groups.empty()) return {}; 
        std::ostringstream in;
        for (auto it = proper_groups.begin(); it != proper_groups.end(); ++it) {
            if (it != proper_groups.begin()) in << ", ";
            in << *it;
        }
        conditions.push_back("group_id IN (" + in.str() + ")");
    }

    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) sql << " AND ";
        sql << conditions[i];
    }

    sql << " ORDER BY coord_y, coord_x";

    pqxx::work txn(conn);
    auto result = txn.exec_params(sql.str(), params);

    std::set<Point2D> points;
    for (const auto& row : result) {
        points.insert({ row[0].as<double>(), row[1].as<double>() });
    }

    return points;
}

std::set<Point2D> QueryBuilder::execute(const QueryNode& node) {
    std::set<Point2D> result;

    if (node.type == QueryNode::Type::Crop) {
        result = exec_crop(node.crops[0]);
    } else if (node.type == QueryNode::Type::And) {
        if (node.children.empty()) return {};

        result = execute(node.children[0]);

        for (size_t i = 1; i < node.children.size(); i++) {
            auto other = execute(node.children[i]);

            std::set<Point2D> intersection;
            std::set_intersection(
                result.begin(), result.end(),
                other.begin(), other.end(),
                std::inserter(intersection, intersection.begin()));
                
            result = std::move(intersection);
        }
    } else if (node.type == QueryNode::Type::Or) {
        for (const auto& child : node.children) {
            auto child_points = execute(child);
            result.insert(child_points.begin(), child_points.end());
        }
    }

    return result;
}

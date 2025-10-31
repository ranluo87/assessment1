#include "query_builder.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

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
    return proper;
}

std::set<Point2D> QueryBuilder::exec_crop(const CropQuery& q) {
    std::ostringstream sql;
    sql << "SELECT coord_x, coord_y FROM inspection_region WHERE "
        << "coord_x >= " << q.region.min_x << " AND coord_x <= " << q.region.max_x
        << " AND coord_y >= " << q.region.min_y << " AND coord_y <= " << q.region.max_y;

    // Bounding box condtions
    // Category condition
    if (q.category) {
        sql << " AND category = " << *q.category;
    }

    // One-of-groups condition
    if (q.one_of_groups) {
        sql << " AND group_id IN (";
        for (size_t i = 0; i < q.one_of_groups->size(); ++i) {
            if (i > 0) sql << ",";
            sql << (*q.one_of_groups)[i];
       }
       sql << ")";
    }
    
    // Proper groups condition
    if (q.proper) {
        auto pg = get_proper_groups();
        if (pg.empty()) {
            return {};
        }
        sql << " AND group_id IN (";
        for (auto it = pg.begin(); it != pg.end(); ++it) {
            if (it != pg.begin()) sql << ",";
            sql << *it;
        }
        sql << ")"; 
    }

    sql << " ORDER BY coord_y, coord_x";
    
    std::cout << sql.str() << std::endl;
    pqxx::work txn(conn);
    auto result = txn.exec(sql.str());

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

#pragma once

struct Point2D{
    double x, y;
    bool operator<(const Point2D& o) const {
        return y < o.y || (y == o.y && x < o.x);
    }
};

struct Region {
    double min_x, min_y, max_x, max_y;
    bool contains(double x, double y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }
};
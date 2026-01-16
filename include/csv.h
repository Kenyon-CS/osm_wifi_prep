#pragma once

#include <string>
#include <string_view>
#include <vector>

// Minimal CSV parser supporting:
// - separator ','
// - quoted fields with "" escaping
// - empty fields
// - no multiline fields
// This is sufficient for Overpass out:csv exports.

inline std::vector<std::string> parseCsvLine(std::string_view line, char sep = ',') {
    std::vector<std::string> out;
    std::string cur;
    cur.reserve(line.size());

    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                // Escaped quote "" inside quoted field
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == sep) {
                out.push_back(cur);
                cur.clear();
            } else if (c == '\r' || c == '\n') {
                // ignore line endings
            } else {
                cur.push_back(c);
            }
        }
    }
    out.push_back(cur);
    return out;
}

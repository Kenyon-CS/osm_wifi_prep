#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "csv.h"

namespace {

std::string trim(std::string s) {
    auto is_ws = [](unsigned char ch) { return ch == ' ' || ch == '\t'; };
    size_t start = 0;
    while (start < s.size() && is_ws(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && is_ws(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool looksLikeHeader(const std::vector<std::string>& cols) {
    // Overpass out:csv typically writes a header if the 2nd parameter is true.
    // e.g., "@type,@id,name,..." or "type,id,name,..." depending on spec.
    if (cols.empty()) return false;
    std::string c0 = cols[0];
    for (auto& ch : c0) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return (c0.find("type") != std::string::npos) || (c0.find("@type") != std::string::npos) || (c0 == "::type");
}

struct Point {
    std::string osm_type;
    std::string osm_id;
    std::string name;
    double lat = std::numeric_limits<double>::quiet_NaN();
    double lon = std::numeric_limits<double>::quiet_NaN();
};

bool parseDouble(const std::string& s, double& out) {
    char* end = nullptr;
    const char* c = s.c_str();
    out = std::strtod(c, &end);
    return end != c;
}

constexpr double kEarthRadiusM = 6371000.0;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

double rad(double deg) { return deg * kDegToRad; }

void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " <input.csv> <output.csv> [options]\n\n"
        << "Reads a CSV exported from overpass-turbo.eu (out:csv ... out center)\n"
        << "and writes a cleaned CSV suitable for algorithm labs.\n\n"
        << "Output columns:\n"
        << "  id,name,lat,lon,x_m,y_m\n\n"
        << "Options:\n"
        << "  --require-name        drop rows without a name\n"
        << "  --min-name-len N       drop rows with name length < N (default 0)\n"
        << "  --dedupe               dedupe by (type,id) (default on)\n"
        << "  --keep-ways-only       keep only ways (drop relations)\n"
        << "  --keep-relations-only  keep only relations (drop ways)\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    std::string inPath = argv[1];
    std::string outPath = argv[2];

    bool requireName = false;
    int minNameLen = 0;
    bool dedupe = true;
    bool waysOnly = false;
    bool relationsOnly = false;

    for (int i = 3; i < argc; ++i) {
        std::string opt = argv[i];
        if (opt == "--require-name") {
            requireName = true;
        } else if (opt == "--min-name-len") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --min-name-len\n";
                return 2;
            }
            minNameLen = std::atoi(argv[++i]);
            if (minNameLen < 0) minNameLen = 0;
        } else if (opt == "--dedupe") {
            dedupe = true;
        } else if (opt == "--keep-ways-only") {
            waysOnly = true;
            relationsOnly = false;
        } else if (opt == "--keep-relations-only") {
            relationsOnly = true;
            waysOnly = false;
        } else if (opt == "--help" || opt == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << opt << "\n";
            usage(argv[0]);
            return 2;
        }
    }

    std::ifstream fin(inPath);
    if (!fin) {
        std::cerr << "Failed to open input: " << inPath << "\n";
        return 1;
    }

    std::vector<Point> pts;
    pts.reserve(4096);

    // Header mapping (best effort). Students may export with different specs.
    // We support either:
    //  - columns including ::type, ::id, name, ::lat, ::lon
    //  - or a fixed order: type,id,name,building,addr...,lat,lon
    std::unordered_map<std::string, size_t> idx;
    bool haveHeader = false;

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        auto cols = parseCsvLine(line, ',');
        if (cols.empty()) continue;

        if (!haveHeader && looksLikeHeader(cols)) {
            haveHeader = true;
            for (size_t c = 0; c < cols.size(); ++c) {
                std::string key = trim(cols[c]);
                for (auto& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                idx[key] = c;
            }
            continue;
        }

        auto get = [&](std::string key) -> std::string {
            for (auto& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            auto it = idx.find(key);
            if (it != idx.end() && it->second < cols.size()) return trim(cols[it->second]);
            return {};
        };

        Point p;
        if (haveHeader) {
            // Try both ::type and @type styles
            p.osm_type = get("::type");
            if (p.osm_type.empty()) p.osm_type = get("@type");
            if (p.osm_type.empty()) p.osm_type = get("type");

            p.osm_id = get("::id");
            if (p.osm_id.empty()) p.osm_id = get("@id");
            if (p.osm_id.empty()) p.osm_id = get("id");

            p.name = get("name");

            std::string slat = get("::lat");
            if (slat.empty()) slat = get("lat");
            std::string slon = get("::lon");
            if (slon.empty()) slon = get("lon");

            if (!parseDouble(slat, p.lat) || !parseDouble(slon, p.lon)) {
                continue;
            }
        } else {
            // No header: assume common export order
            // type,id,name,building,addr:housenumber,addr:street,lat,lon
            if (cols.size() < 8) continue;
            p.osm_type = trim(cols[0]);
            p.osm_id = trim(cols[1]);
            p.name = trim(cols[2]);
            if (!parseDouble(trim(cols[6]), p.lat) || !parseDouble(trim(cols[7]), p.lon)) {
                continue;
            }
        }

        if (waysOnly && p.osm_type != "way") continue;
        if (relationsOnly && p.osm_type != "relation") continue;

        if (requireName && p.name.empty()) continue;
        if (static_cast<int>(p.name.size()) < minNameLen) continue;

        pts.push_back(std::move(p));
    }

    if (pts.empty()) {
        std::cerr << "No usable rows found. Check that your export includes lat/lon (use 'out center').\n";
        return 1;
    }

    // Compute origin for local meters conversion
    double lat0 = 0.0, lon0 = 0.0;
    for (const auto& p : pts) {
        lat0 += p.lat;
        lon0 += p.lon;
    }
    lat0 /= static_cast<double>(pts.size());
    lon0 /= static_cast<double>(pts.size());

    const double lat0r = rad(lat0);

    // Write output
    std::ofstream fout(outPath);
    if (!fout) {
        std::cerr << "Failed to open output: " << outPath << "\n";
        return 1;
    }

    fout << "id,name,lat,lon,x_m,y_m\n";

    std::unordered_set<std::string> seen;
    seen.reserve(pts.size() * 2);

    size_t written = 0;
    for (const auto& p : pts) {
        std::string key = p.osm_type + ":" + p.osm_id;
        if (dedupe) {
            if (!seen.insert(key).second) continue;
        }

        double x = (rad(p.lon - lon0)) * std::cos(lat0r) * kEarthRadiusM;
        double y = (rad(p.lat - lat0)) * kEarthRadiusM;

        // If no name, give a stable fallback
        std::string name = p.name.empty() ? key : p.name;

        fout << key << ',';
        // Quote the name if it contains comma or quote
        bool needQuotes = (name.find(',') != std::string::npos) || (name.find('"') != std::string::npos);
        if (needQuotes) {
            fout << '"';
            for (char c : name) {
                if (c == '"') fout << '"';
                fout << c;
            }
            fout << '"';
        } else {
            fout << name;
        }
        fout << ',' << p.lat << ',' << p.lon << ',' << x << ',' << y << "\n";
        ++written;
    }

    std::cerr << "Read points: " << pts.size() << "\n";
    std::cerr << "Wrote points: " << written << "\n";
    std::cerr << "Origin (lat,lon): " << lat0 << "," << lon0 << "\n";

    return 0;
}

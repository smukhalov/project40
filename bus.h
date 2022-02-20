#pragma once
#include <vector>
#include <string>
#include <string_view>
#include "stop.h"

struct Bus {
	struct Stop{
		size_t stop_id;
		std::string stop_name;

		bool operator == (const Stop& other) const {
			return stop_name == other.stop_name && stop_id == other.stop_id;
		}
	};

	struct StopHasher {
		size_t operator() (const Stop& bs) const {
			size_t x = 2'946'901;
			return shash(bs.stop_name) * x + size_t_hash(bs.stop_id);
		}

		std::hash<std::string> shash;
		std::hash<size_t> size_t_hash;
	};

	std::string name;
	size_t unique_stops_count;
	std::vector<Stop> stops;
	RouteType route_type;

	const size_t GetSize() const;
};



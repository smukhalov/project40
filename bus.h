#pragma once
#include <vector>
#include <string>
#include <string_view>
#include "stop.h"

struct Bus {
	std::string name;
	size_t unique_stops_count;
	std::vector<std::string> stops;
	RouteType route_type;

	const size_t GetSize() const;
};



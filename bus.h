#pragma once
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include "stop.h"

struct Bus {
	std::string name;
	size_t unique_stops_count;
	std::map<size_t, std::string> stops;
	RouteType route_type;

	size_t route_size;

	const size_t GetSize() const;
	/*const std::string GetFirstStop() const;
	const std::string GetLastStop() const;
	const std::optional<std::string> GetNextStop(const std::string& stop_name) const;*/
};



#include <cassert>
#include <algorithm>
#include <optional>
#include "bus.h"

const size_t Bus::GetSize() const {
	if(route_type == RouteType::Round){
		return stops.size();
	}

	return 2*stops.size() - 1;
}
/*
const std::string Bus::GetFirstStop() const {
	assert(stops.size() > 0);

	return stops.at(0);
}

const std::string Bus::GetLastStop() const {
	assert(stops.size() > 0);

	return stops.at(stops.size() - 1);
}

const std::optional<std::string> Bus::GetNextStop(const std::string& stop_name) const {
	assert(stops.size() > 0);

	auto it = std::find_if(stops.begin(), stops.end(), [&](const auto& other){
		return other.second == stop_name;
	});

	if(it == stops.end()){
		throw std::invalid_argument("Stop not found " + stop_name);
	}

	if(std::next(it) == stops.end()){
		return std::nullopt;
	}

	return std::next(it)->second;
}*/





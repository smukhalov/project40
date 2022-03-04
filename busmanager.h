#pragma once
#include <unordered_map>
#include <map>
#include <vector>
#include <set>
#include <unordered_set>
#include <sstream>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <cmath>
#include "bus.h"
#include "command.h"
#include "route.h"
#include "json.h"
#include "routing_settings.h"
#include "graph.h"
#include "router.h"

class BusManager {
public:
	BusManager();
	BusManager& Read(std::istream& in = std::cin);
	void WriteResponse(std::ostream& out = std::cout) const;

private:
	size_t last_init_id;
	bool logging = true;

	struct StopBuses{
		std::string stop_name;
		std::set<std::string> bus_set;

		bool operator == (const StopBuses& other) const {
			return stop_name == other.stop_name && bus_set == other.bus_set;
		}
	};

	struct StopBusesHasher {
		size_t operator() (const StopBuses& e) const {
			std::stringstream ss;
			for(const std::string& bus_name: e.bus_set){
				ss << bus_name << ' ';
			}

			size_t x = 2'946'901;
			return shash(e.stop_name)*x + shash(ss.str());
		}

		std::hash<std::string> shash;
	};

	std::unordered_map<StopBuses, size_t, StopBusesHasher> stopbuses_to_vertex;
	std::unordered_map<size_t, StopBuses> vertex_to_stopbuses;

	struct Stage {
		std::string stop_from;
		std::string stop_to;
		double distance;

		bool operator == (const Stage& other) const {
			return stop_from == other.stop_from && stop_to == other.stop_to
					&& abs(distance - other.distance) < 0.000001 ;
		}
	};

	struct StageHasher {
		size_t operator() (const Stage& e) const {
			size_t x = 2'946'901;
			return shash(e.stop_from)*x*x + shash(e.stop_to)*x + dhash(e.distance);
		}

		std::hash<std::string> shash;
		std::hash<double> dhash;
	};

	mutable std::unordered_map<Stage, std::unordered_set<std::string>, StageHasher> stage_to_buses;

	struct Edge {
	    size_t from;
	    size_t to;
	    double distance;

	    bool operator == (const Edge& other) const {
			return from == other.from && to == other.to;
		}
	  };

	struct EdgeHasher {
		size_t operator() (const Edge& e) const {
			size_t x = 2'946'901;
			return shash(e.from) * x + shash(e.to);
		}

		std::hash<std::size_t> shash;
	};

	std::unordered_set<Edge, EdgeHasher> edges;

	RoutingSettings settings;
	std::unordered_map<std::string, Bus> buses;
	std::unordered_map<std::string, Stop> stops;
	std::unordered_map<std::string, std::set<std::string>> stop_to_buses;
	std::unordered_map<StopPair, size_t, StopPairHasher> stop_distances;

	std::vector<std::unique_ptr<Command>> commands;

	double GetDistanceByGeo(const Bus& bus) const;
	size_t GetDistanceByStops(const Bus& bus) const;
	size_t GetDistanceByStops(const Bus& bus, bool forward) const;

	BusManager& ReadData(const std::vector<Json::Node>& node);
	BusManager& ReadRequest(const std::vector<Json::Node>& node);
	BusManager& ReadSettings(const std::map<std::string, Json::Node>& node);

	Route BuildBestRoute(const RouteCommand& command,
		const Graph::DirectedWeightedGraph<double>& graph,
		Graph::Router<double>& router) const;

	double GetDistance(std::vector<std::string>::const_iterator it) const;
	void AddEdge(const Edge& edge);

	double GetDistance(const std::string& stop_name, const std::string& stop_name_next) const;

	void FillEdges();
};

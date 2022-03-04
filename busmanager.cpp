#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <iterator>
#include <optional>
#include <unordered_set>
#include <utility>
#include <unordered_set>
#include <fstream>
#include "stringhelper.h"
#include "busmanager.h"

BusManager::BusManager(): last_init_id(0){}

BusManager& BusManager::ReadSettings(const std::map<std::string, Json::Node>& node) {

	for(const auto& [node_name, value]: node) {
		if(node_name == "bus_wait_time"){
			settings.bus_wait_time = (value.IsDouble()) ? value.AsDouble() : value.AsInt();
		} else if(node_name == "bus_velocity") {
			//В метрах/минуту
			settings.bus_velocity = ((value.IsDouble()) ? value.AsDouble() : value.AsInt()) * 1000.0 / 60.0;
 		} else {
 			throw std::invalid_argument("BusManager::ReadSettings unsupported argument name " + node_name);
 		}
	}
	return *this;
}

BusManager& BusManager::ReadData(const std::vector<Json::Node>& node){
	std::vector<std::string> data_types;
	for(const auto& node_array: node){
		const auto& node_map = node_array.AsMap();
		for(const auto& [node_name, value]: node_map) {
			if(node_name == "type"){
				data_types.push_back(value.AsString());
				break;
			}
		}
	}

	auto it_data_types = data_types.begin();
	for(const auto& node_array: node){
		const auto& node_map = node_array.AsMap();

		if(*it_data_types == "Stop"){
			Stop stop;
			stop.id = stops.size();

			std::unordered_map<std::string, size_t> other_stops;

			for(const auto& [node_name, value]: node_map) {
				//std::string node_name = item_map.first;
				if(node_name == "name"){
					stop.name = value.AsString();
				} else if(node_name == "latitude"){
					stop.point.latitude = value.IsDouble() ? value.AsDouble() : value.AsInt();
				} else if(node_name == "longitude"){
					stop.point.longitude = value.IsDouble() ? value.AsDouble() : value.AsInt();
				} else if(node_name == "road_distances"){
					const std::map<std::string, Json::Node>& item_stops = value.AsMap();
					for(const auto& [stop_name, stop_distance]: item_stops){
						other_stops.emplace( std::make_pair(stop_name, stop_distance.AsInt()) );
					}
				} else if(node_name == "type"){
					continue; //
				} else {
					throw std::invalid_argument("BusManager::ReadData: Stop unsupported argument name " + node_name);
				}
			}

			for(const auto& [other_stop_name, other_stop_distance]: other_stops){
				stop_distances.emplace(std::make_pair(StopPair{stop.name, other_stop_name}, other_stop_distance));
			}

			stops.emplace(std::pair<std::string, Stop>{stop.name, stop});
		} else if(*it_data_types == "Bus"){
			Bus bus;
			for(const auto& [node_name, value]: node_map) {
				if(node_name == "name"){
					bus.name = value.AsString();
				} else if(node_name == "is_roundtrip"){
					bus.route_type = value.AsBool() ? RouteType::Round : RouteType::Line;
				} else if(node_name == "stops"){
					const std::vector<Json::Node>& item_stops = value.AsArray();
					std::unordered_set<std::string> unique_stops;

					//size_t bus_stop_id = 0;
					for(const auto& item_stop: item_stops){
						//bus.stops.push_back(item_stop.AsString());
						bus.stops.insert({bus.stops.size(), item_stop.AsString()});
						unique_stops.insert(item_stop.AsString());
					}
					bus.unique_stops_count = unique_stops.size();
				} else if(node_name == "type"){
					continue;
				} else {
					throw std::invalid_argument("BusManager::ReadData: Bus unsupported argument name " + node_name);
				}
			}

			buses.emplace(std::pair<std::string, Bus>{bus.name, bus});
			for(const auto& [stop_id,  stop_name]: bus.stops){
				stop_to_buses[stop_name].insert(bus.name);
			}
		} else {
			throw std::invalid_argument("BusManager::ReadData: unsupported data type " + *it_data_types);
		}

		it_data_types++;
	}

	return *this;
}

BusManager& BusManager::ReadRequest(const std::vector<Json::Node>& node){
	for(const auto& node_array: node){
		const auto& node_map = node_array.AsMap();
		std::unique_ptr<Command> command;

		for(const auto& [node_name, value]: node_map) {
			if(node_name == "type"){
				const std::string type = value.AsString();
				if(type == "Bus"){
					command = std::make_unique<BusCommand>();
				} else if(type == "Stop"){
					command = std::make_unique<StopCommand>();
				} else if(type == "Route"){
					command = std::make_unique<RouteCommand>();
				}else {
					throw std::invalid_argument("BusManager::ReadRequest: unsupported command type " + type);
				}

				commands.push_back(move(command));
				break;
			}
		}
	}

	auto it_command_types = commands.begin();
	for(const auto& node_array: node){
		const auto& node_map = node_array.AsMap();

		for(const auto& [node_name, value]: node_map) {
			if(node_name == "id"){
				(*it_command_types)->id = value.AsInt();
				continue;
			} else if(node_name == "type"){
				continue;
			}

			if((*it_command_types)->GetType() == CommandType::Bus){
				if(node_name == "name"){
					((BusCommand*)(it_command_types->get()))->name = value.AsString();
				}
			} else if((*it_command_types)->GetType() == CommandType::Stop){
				if(node_name == "name"){
					((StopCommand*)(it_command_types->get()))->name = value.AsString();
				}
			} else if((*it_command_types)->GetType() == CommandType::Route){
				if(node_name == "from"){
					((RouteCommand*)(it_command_types->get()))->stop_from = value.AsString();
				} else if(node_name == "to") {
					((RouteCommand*)(it_command_types->get()))->stop_to = value.AsString();
				} else {
					throw std::invalid_argument("BusManager::ReadRequest: RouteCommand unsupported key: " + node_name);
				}
			} else {
				throw std::invalid_argument("BusManager::ReadRequest: unsupported command type");
			}
		}

		it_command_types++;
	}

	return *this;
}

BusManager& BusManager::Read(std::istream& in){
	Json::Document doc = Json::Load(in);
	const Json::Node& node_root = doc.GetRoot();
	const auto& root_map = node_root.AsMap();

	for(const auto& [node_name, value]: root_map){
		if(node_name == "base_requests"){
			ReadData(value.AsArray());
		} else if(node_name == "stat_requests"){
			ReadRequest(value.AsArray());
		} else if(node_name == "routing_settings"){
			ReadSettings(value.AsMap());
		} else {
			throw std::invalid_argument("Incorrect root node_name - " + node_name);
		}
	}

	for(auto& [bus_name, bus]: buses){
		bus.route_size = GetDistanceByStops(bus);
	}

	std::vector<std::set<std::string>> bus_vector;
	std::unordered_set<std::string> mark_bus;

	for(auto it = buses.begin(); it != buses.end(); ++it){
		auto it_next = std::next(it);
		if(std::next(it) == buses.end()){
			continue;
		}

		if(auto it_mark = std::find(mark_bus.begin(), mark_bus.end(), it->first); it_mark != mark_bus.end()){
			continue;
		}

		std::set<std::string> bus_pack;
		bus_pack.insert(it->first);

		for(auto it1 = it_next; it1 != buses.end(); ++it1){
			if(it1->second.stops == it->second.stops){
				mark_bus.insert(it1->first);
				bus_pack.insert(it1->first);
			}
		}

		if(bus_pack.size() > 1){
			bus_vector.push_back(bus_pack);
		}

		mark_bus.insert(it->first);
	}

	FillEdges();

	//Sheremetyevo
	//Vnukovo

	return *this;
}

void BusManager::WriteResponse(std::ostream& out) const {
	size_t vertex_count = last_init_id;
	Graph::DirectedWeightedGraph<double> graph(vertex_count);

	std::cout << "vertex_count - " << vertex_count << std::endl;
	std::cout << "edges_count - " << edges.size() << std::endl;

	//return;
	//std::unordered_map<Stage, std::unordered_set<std::string>, StageHasher> stage_to_buses;
	for(const auto& [stage, bus_set]: stage_to_buses){
		std::cout << stage.stop_from << " - " << stage.stop_to
				<< "; distance - " << stage.distance << "; buses - ";

		for(const auto& bus_name: bus_set){
			std::cout << bus_name << ' ';
		}
		std::cout << "\n";
	}



	//return;
	if(logging){
		std::cout << "\nedges list\n";
	}
	for(const auto& [from, to, distance]: edges){
		if(logging){
			std::cout << "from - " << from << ", to - " << to << "; distance - " << distance << std::endl;
		}
		graph.AddEdge({from, to, distance});
	}
	return;
	if(logging){
		std::cout << std::endl;
		/*std::cout << "vertex_to_bus_stop count - " << vertex_to_bus_stop.size() << "\n";
		for(const auto& [vertex_id, bus_to_stop_set]: vertex_to_bus_stop){
			std::cout << "vertex_id - " << vertex_id << std::endl;
			for(const auto& bus_to_stop: bus_to_stop_set){
				std::cout << "\t\tbus_to_stop.bus_name - " << bus_to_stop.bus_name
									<< "; bus_to_stop_name - " << bus_to_stop.stop_name << std::endl;
			}
		}*/

		/*std::cout << std::endl;
		std::cout << "stop_to_bus_vertex count - " << stop_to_bus_vertex.size() << "\n";
		for(const auto& [stop_name, bus_vertex_set]: stop_to_bus_vertex){
			std::cout << "stop_name - " << stop_name  << std::endl;
			for(const auto& bus_vertex: bus_vertex_set){
				std::cout << "\t\t" << "vertex_id - " << bus_vertex.vertex_id << "; bus_name - " << bus_vertex.bus_name << std::endl;
			}
		}*/

		/*std::cout << std::endl;
		std::cout << "bus_stop_to_vertex count - " << bus_stop_to_vertex.size() << "\n";
		for(const auto& [bus_stop, vertex_id]: bus_stop_to_vertex){
			std::cout << "vertex_id - " << vertex_id << ", bus_name - " << bus_stop.bus_name
					<< "; stop_name - " << bus_stop.stop_name << std::endl;
		}*/
	}

	Graph::Router<double> router(graph);
	//return;

	//out << std::fixed << std::setprecision(6) << "[\n";
	out << "[\n";

	size_t count = commands.size();
	size_t n = 0;
	for(const auto& command: commands){
		out << "\t{\n";
		out << "\t\t\"request_id\": " << command->id << ",\n";
		if(command->GetType() == CommandType::Bus){
			if(const auto it = buses.find(((BusCommand*)(command.get()))->name); it == buses.end()){
				out << "\t\t\"error_message\": \"not found\"\n";
			} else {
				//size_t distance_by_stops = GetDistanceByStops(it->second);
				out << "\t\t\"stop_count\": " << it->second.GetSize() << ",\n";
				out << "\t\t\"unique_stop_count\": " << it->second.unique_stops_count << ",\n";
				out << "\t\t\"route_length\": " << (it->second).route_size << ",\n";
				out << "\t\t\"curvature\": " << (it->second).route_size / GetDistanceByGeo(it->second) << "\n";
			}
		} else if(command->GetType() == CommandType::Stop){
			if(const auto it = stop_to_buses.find(((StopCommand*)(command.get()))->name); it == stop_to_buses.end()){
				out << "\t\t\"error_message\": \"not found\"\n";
			} else if(it->second.size() == 0) {
				out << "\t\t\"buses\": []\n";
			} else {
				size_t bus_count = it->second.size();
				size_t b = 0;
				out << "\t\t\"buses\": [\n";
				for(const auto& item: it->second){
					out << "\t\t\t\"" << item << "\"";
					if(++b < bus_count){
						out << ",\n";
					}
				}
				out << "\n\t\t]\n";
			}
		} else if(command->GetType() == CommandType::Route){
			RouteCommand rc = *(RouteCommand*)(command.get());
			if(rc.stop_from == rc.stop_to){
				out << "\t\t\"total_time\": 0,\n\t\t\"items\": []\n";
			} else {
				Route route = BuildBestRoute(rc, graph, router);

				if(route.items.size() == 0){
					out << "\t\t\"error_message\": \"not found\"\n";
				} else {
					out << "\t\t\"total_time\": " << route.total_time << ",\n";

					size_t items_count = route.items.size();
					size_t n = 0;
					out << "\t\t\"items\": [\n";
					for(const auto& item: route.items){
						item->Print(out);
						if(++n < items_count){
							out << ",\n";
						}
					}
					out << "\n\t\t]\n";
				}
			}
		}
		out << "\t}";
		if(++n < count) {
			out << ",\n";
		}
	}
	out << "\n]";
}

Route BusManager::BuildBestRoute(const RouteCommand& command,
			const Graph::DirectedWeightedGraph<double>& graph,
			Graph::Router<double>& router) const {

	Route route;
	route.total_time = -1.0;

	return route;

	/*std::vector<Graph::VertexId> vertex_from_list;
	if(auto it = stop_to_vertex_list.find(command.stop_from); it != stop_to_vertex_list.end()){
		const std::unordered_set<size_t>& vertex_list = stop_to_vertex_list.at(command.stop_from);
		std::copy(vertex_list.begin(), vertex_list.end(), std::back_inserter(vertex_from_list));
	}

	if(vertex_from_list.size() == 0){
		return route;
	}

	std::vector<Graph::VertexId> vertex_to_list;
	//if(stop_to_vertex_list.count(command.stop_to) > 0){
	if(auto it = stop_to_vertex_list.find(command.stop_to); it != stop_to_vertex_list.end()){
		const std::unordered_set<size_t>& vertex_list = stop_to_vertex_list.at(command.stop_to);
		std::copy(vertex_list.begin(), vertex_list.end(), std::back_inserter(vertex_to_list));
	}

	if(vertex_to_list.size() == 0){
		return route;
	}

	std::vector<Graph::EdgeId> route_edges;
	for(const Graph::VertexId vertex_from: vertex_from_list){
		for(const Graph::VertexId vertex_to: vertex_to_list){
			//std::cout << "Ищем маршрут: " << vertex_from << " - " << vertex_to << std::endl;

			auto route_info = router.BuildRoute(vertex_from, vertex_to);
			if(!route_info || route_info->edge_count < 1){
				continue;
			} else if(route.total_time < 0 || route_info->weight < route.total_time) {
				route_edges.clear();

				route.total_time = route_info->weight;
				for(size_t i = 0; i < route_info->edge_count; i++) {
					route_edges.push_back(router.GetRouteEdge(route_info->id, i));
				}

				router.ReleaseRoute(route_info->id);
			}
		}
	}

	if(route_edges.size() == 0){
		return route;
	}

	route.total_time += settings.bus_wait_time;

	RouteItemWait rw;
	rw.stop_name = command.stop_from;
	rw.bus_wait_time = settings.bus_wait_time;

	route.items.push_back(std::make_shared<RouteItemWait>(rw));

	auto it_route_edges_begin = route_edges.begin();
	auto it_route_edges_end = route_edges.end();

	while(true) {
		auto it_route_edges_end_current = std::find_if(it_route_edges_begin, it_route_edges_end, [&](const Graph::EdgeId& edge_id) {
			const Graph::Edge<double>& edge = graph.GetEdge(edge_id);
			return edge.route_item_type == RouteItemType::Wait;
		});

		const Graph::Edge<double>& edge = graph.GetEdge(*it_route_edges_begin);
		std::string stop_name_from = vertex_to_stop.at(edge.from);
		std::string stop_name_to = vertex_to_stop.at(edge.to);

		std::set<std::string> buses_from = stop_to_buses.at(stop_name_from);
		std::set<std::string> bus_intersection;

		uint32_t span_count = std::distance(it_route_edges_begin, it_route_edges_end_current);
		double bus_move_time = edge.weight;

		for(auto it = ++it_route_edges_begin; it != it_route_edges_end_current; ++it ){
			const Graph::Edge<double>& edge = graph.GetEdge(*it);
			bus_move_time += edge.weight;

			stop_name_from = vertex_to_stop.at(edge.from);
			std::set<std::string> buses_current = stop_to_buses.at(stop_name_from);

			std::set_intersection(buses_from.begin(), buses_from.end(),
					buses_current.begin(), buses_current.end(),
					std::inserter(bus_intersection, bus_intersection.begin()));

			buses_from.clear();
			std::copy(bus_intersection.begin(), bus_intersection.end(), std::inserter(buses_from, buses_from.begin()));
			bus_intersection.clear();

			stop_name_to = vertex_to_stop.at(edge.to);
		}

		RouteItemBus rb;
		rb.bus_number = *buses_from.begin();
		rb.span_count = span_count;
		rb.bus_move_time = bus_move_time;
		route.items.push_back(std::make_shared<RouteItemBus>(rb));

		if(it_route_edges_end_current == it_route_edges_end){
			break;
		}

		RouteItemWait rw;
		rw.stop_name = stop_name_to;
		rw.bus_wait_time = settings.bus_wait_time;

		route.items.push_back(std::make_shared<RouteItemWait>(rw));
		it_route_edges_begin = it_route_edges_end_current + 1;
	}*/

	return route;
}

double BusManager::GetDistance(std::vector<std::string>::const_iterator it) const {
	double bus_time;

	if(auto it_distance_b = stop_distances.find(StopPair{*it, *(it+1)}); it_distance_b != stop_distances.end()) {
		bus_time = it_distance_b->second / settings.bus_velocity;
	} else if(auto it_distance_e = stop_distances.find(StopPair{*(it+1), *it});	it_distance_e != stop_distances.end()) {
		bus_time = it_distance_e->second / settings.bus_velocity;
	} else {
		throw std::invalid_argument("Not found distance " + (*it) + " and " + (*(it+1)));
	}

	return bus_time;
}

size_t BusManager::GetDistanceByStops(const Bus& bus) const {
	if(bus.stops.size() < 2){
		return 0;
	}

	size_t result = GetDistanceByStops(bus, true);
	if(bus.route_type == RouteType::Line){
		result += GetDistanceByStops(bus, false);
	}

	return result;
}

size_t BusManager::GetDistanceByStops(const Bus& bus, bool forward) const {
	size_t result = 0;
	size_t stop_count =	bus.stops.size();

	auto it_end = stop_distances.end();
	for(size_t i = 0; i < stop_count-1; i++){
		size_t first_stop = forward ? i : i + 1;
		size_t second_stop = forward ? i + 1 : i;

		std::string stop_name_1 = bus.stops.at(first_stop);
		std::string stop_name_2 = bus.stops.at(second_stop);

		auto it = stop_distances.find({stop_name_1, stop_name_2});
		if(it == it_end){
			it = stop_distances.find({stop_name_2, stop_name_1});
			if(it == it_end){
				throw std::invalid_argument("[" + stop_name_1 + ", " + stop_name_2 + "] not found");
			}
		}

		stage_to_buses[{ stop_name_1, stop_name_2, it->second / settings.bus_velocity }].insert(bus.name);
		result += it->second;
	};

	return result;
}

double BusManager::GetDistanceByGeo(const Bus& bus) const {
	double result = 0.0;
    size_t stop_count =	bus.stops.size();
	if(bus.stops.size() < 2){
		return 0.0;
	}

	const std::map<size_t, std::string>& stops_for_bus = bus.stops;
	auto it_end = stops.end();

	for(size_t i = 0; i < stop_count - 1; i++){
		const std::string& stop_name_1 = stops_for_bus.at(i);
		const std::string& stop_name_2 = stops_for_bus.at(i+1);

		auto it1 = stops.find(stop_name_1);
		if(it1 == it_end){
			throw std::invalid_argument(stop_name_1 + " not found");
		}

		auto it2 = stops.find(stop_name_2);
		if(it2 == it_end){
			throw std::invalid_argument(stop_name_2 + " not found");
		}

		result += Distance(it1->second.point, it2->second.point).Length();
	}

	if(bus.route_type == RouteType::Line){
		result *= 2;
	}

	return result;
}

void BusManager::AddEdge(const Edge& edge){
	if(edge.from == edge.to){
		return;
	}

	auto it = edges.find({edge.from, edge.to});
	if(it == edges.end()){
		edges.insert(edge);
		return;
	}

	if(it->distance < edge.distance){
		return;
	}

	auto nh = edges.extract(it);
	nh.value() = edge;
	edges.insert(move(nh));
}

double BusManager::GetDistance(const std::string& stop_name, const std::string& stop_name_next) const {
	auto it_stop = stop_distances.find({stop_name, stop_name_next});

	if(it_stop == stop_distances.end()){
		it_stop = stop_distances.find({stop_name, stop_name_next});
		if(it_stop == stop_distances.end()){
			throw std::invalid_argument("GetDistnace. distance [" + stop_name + ", " + stop_name_next + "] not found");
		}
	}

	return it_stop->second / settings.bus_velocity;
}

void BusManager::FillEdges(){
	//std::unordered_map<StopBuses, size_t, StopBusesHasher> stopbuses_to_vertex;
	//std::unordered_map<size_t, StopBuses> vertex_to_stopbuses;

	for(const auto& [stage, bus_set]: stage_to_buses){
		for(const auto& bus_name: bus_set){
			StopBuses stop_buses_from = {stage.stop_from, {bus_name}};

			size_t vertex_from = -1;
			if(auto it = stopbuses_to_vertex.find(stop_buses_from); it == stopbuses_to_vertex.end()){
				vertex_from = last_init_id++;

				stopbuses_to_vertex.insert({stop_buses_from, vertex_from});
				vertex_to_stopbuses.insert({vertex_from, stop_buses_from});
			} else {
				vertex_from = it->second;
			}

			StopBuses stop_buses_to = {stage.stop_to, {bus_name}};

			size_t vertex_to = -1;
			if(auto it = stopbuses_to_vertex.find(stop_buses_to); it == stopbuses_to_vertex.end()){
				vertex_to = last_init_id++;

				stopbuses_to_vertex.insert({stop_buses_to, vertex_to});
				vertex_to_stopbuses.insert({vertex_to, stop_buses_to});
			} else {
				vertex_to = it->second;
			}

			AddEdge({vertex_from, vertex_to, stage.distance});
		}
	}

	for(const auto& [stop_buses, vertex]: stopbuses_to_vertex){
		std::cout << "stop_buses: [" << stop_buses.stop_name << ", " << *stop_buses.bus_set.begin()
				<< ", " << vertex << "]" << std::endl;
	}
}

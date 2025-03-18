#include "oip_comms.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OIPComms::OIPComms() {
	// maybe using RefCounted here instead will make warnings go away?

	print("Read thread start");
	work_thread.instantiate();
	work_thread->start(callable_mp(this, &OIPComms::process_work));

	print("Watchdog thread start");
	watchdog_thread.instantiate();
	watchdog_thread->start(callable_mp(this, &OIPComms::watchdog));
}

OIPComms::~OIPComms() {
	cleanup_tag_groups();

	watchdog_thread_running = false;
	work_thread_running = false;
	tag_group_queue.shutdown();

	work_thread->wait_to_finish();
	watchdog_thread->wait_to_finish();
	print("Threads shutdown");
}

void OIPComms::cleanup_tag_groups() {
	for (auto const &x : tag_groups) {
		const String tag_group_name = x.first;
		cleanup_tag_group(tag_group_name);
	}
}

void OIPComms::cleanup_tag_group(const String &tag_group_name) {
	TagGroup &tag_group = tag_groups[tag_group_name];
	if (tag_group.protocol == "opc_ua") {
		for (auto &x : tag_group.opc_ua_tags) {
			OpcUaTag &tag = x.second;
			UA_Variant_clear(&tag.value);
		}

		if (tag_group.client != nullptr) {
			UA_Client_delete(tag_group.client);
		}

	} else {
		for (auto &x : tag_group.plc_tags) {
			PlcTag &tag = x.second;
			plc_tag_destroy(tag.tag_pointer);
		}
	}
}

void OIPComms::watchdog() {
	while (watchdog_thread_running) {
		if (!scene_signals_set) {
			SceneTree *main_scene = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
			if (main_scene != nullptr) {
				main_scene->connect("process_frame", callable_mp(this, &OIPComms::process));
				print("Scene signals set");
				scene_signals_set = true;
			}
		}

		OS::get_singleton()->delay_msec(500);
	}
}

void OIPComms::process_work() {
	while (work_thread_running) {
		// this pop operation is blocking - thread will sleep until a request comes along
		String tag_group_name = tag_group_queue.pop();

		flush_all_writes();

		if (tag_group_name.is_empty() && !work_thread_running)
			break;

		if (tag_groups.find(tag_group_name) != tag_groups.end()) {
			process_tag_group(tag_group_name);
		} else {
			if (tag_group_name.is_empty()) {
				print("Processing writes (no tag groups to be updated)");
			} else {
				print("Tag group not found: " + tag_group_name);
			}
		}
	}
}

void OIPComms::queue_tag_group(const String &tag_group_name) {
	tag_group_queue.push(tag_group_name);
}

void OIPComms::flush_all_writes() {
	while (!write_queue.empty()) {
		WriteRequest write_req = write_queue.front();
		write_queue.pop();
		process_write(write_req);
	}
}

// not currently used but might considering flushing one write for each read
void OIPComms::flush_one_write() {
	if (!write_queue.empty()) {
		WriteRequest write_req = write_queue.front();
		write_queue.pop();
		process_write(write_req);
	}
}

void OIPComms::opc_write(const String &tag_group_name, const String &tag_path) {
	TagGroup &tag_group = tag_groups[tag_group_name];

	UA_StatusCode client_status;
	UA_Client_getState(tag_group.client, nullptr, nullptr, &client_status);
	if (client_status != UA_STATUSCODE_GOOD)
		return;

	OpcUaTag &tag = tag_group.opc_ua_tags[tag_path];
	if (!tag.initialized)
		return;

	UA_StatusCode ret_val = UA_Client_writeValueAttribute(tag_group.client, tag.node_id, &(tag.value));
	if (ret_val != UA_STATUSCODE_GOOD) {
		print("OIP Comms: Failed to write tag value for " + tag_path + " with status code " + String(UA_StatusCode_name(ret_val)));
	}
}

#define OIP_OPC_SET(a, b, c, d) \
void OIPComms::opc_tag_set_##a(const String &tag_group_name, const String &tag_path, const godot::Variant value) { \
	if (value.get_type() == Variant::##d) { \
		OpcUaTag &tag = tag_groups[tag_group_name].opc_ua_tags[tag_path]; \
		if (!tag.initialized) return; \
		b raw_value = (b)value; \
		UA_StatusCode ret_val = UA_Variant_setScalarCopy(&(tag.value), &raw_value, &UA_TYPES[UA_TYPES_##c]); \
		if (ret_val != UA_STATUSCODE_GOOD) \
			print("OIP Comms: Failed to cast data on write for " + tag_path); \
		opc_write(tag_group_name, tag_path); \
	} else { \
		print("OIP Comms: Supplied data type incorrect for " + tag_path); \
	} \
}

/* Data marshalling is a giant PITA in this project
libplctag, open62541 and Godot have different names for each of the fundamental data types
Godot variants support direct casting, while open62541 needs to use UA_Variant_setScalarCopy()
*/
OIP_OPC_SET(bit, bool, BOOLEAN, BOOL)
OIP_OPC_SET(uint64, uint64_t, UINT64, INT)
OIP_OPC_SET(int64, int64_t, INT64, INT)
OIP_OPC_SET(uint32, uint32_t, UINT32, INT)
OIP_OPC_SET(int32, int32_t, INT32, INT)
OIP_OPC_SET(uint16, uint16_t, UINT16, INT)
OIP_OPC_SET(int16, int16_t, INT16, INT)
OIP_OPC_SET(uint8, uint8_t, UINT16, INT) // there's no 8 bit integer types in OPC UA
OIP_OPC_SET(int8, int8_t, INT16, INT)
OIP_OPC_SET(float64, double, DOUBLE, FLOAT)
OIP_OPC_SET(float32, float, FLOAT, FLOAT)

#define OIP_OPC_SET_CALL(a) \
if (tag_group.protocol == "opc_ua") { \
	opc_tag_set_##a(write_req.tag_group_name, write_req.tag_name, write_req.value); \
} else { \
	if (tag_pointer != -1) plc_tag_set_##a(tag_pointer, 0, write_req.value); \
}

void OIPComms::process_write(const WriteRequest &write_req) {
	TagGroup &tag_group = tag_groups[write_req.tag_group_name];

	int32_t tag_pointer = -1;
	if (tag_group.protocol != "opc_ua") tag_pointer = tag_group.plc_tags[write_req.tag_name].tag_pointer;

	switch (write_req.instruction) {
		case 0:
			OIP_OPC_SET_CALL(bit)
			break;
		case 1:
			OIP_OPC_SET_CALL(uint64)
			break;
		case 2:
			OIP_OPC_SET_CALL(int64)
			break;
		case 3:
			OIP_OPC_SET_CALL(uint32)
			break;
		case 4:
			OIP_OPC_SET_CALL(int32)
			break;
		case 5:
			OIP_OPC_SET_CALL(uint16)
			break;
		case 6:
			OIP_OPC_SET_CALL(int16)
			break;
		case 7:
			OIP_OPC_SET_CALL(uint8)
			break;
		case 8:
			OIP_OPC_SET_CALL(int8)
			break;
		case 9:
			OIP_OPC_SET_CALL(float64)
			break;
		case 10:
			OIP_OPC_SET_CALL(float32)
			break;
	}
	if (tag_group.protocol != "opc_ua") {
		if (plc_tag_write(tag_pointer, timeout) == PLCTAG_STATUS_OK) {
			tag_groups[write_req.tag_group_name].plc_tags[write_req.tag_name].dirty = true;
		} else {
			print("Failed to write tag: " + write_req.tag_name);
		}
	}
}

void OIPComms::process_tag_group(const String &tag_group_name) {
	TagGroup &tag_group = tag_groups[tag_group_name];
	if (tag_group.protocol == "opc_ua") {
		process_opc_ua_tag_group(tag_group_name);
	} else {
		process_plc_tag_group(tag_group_name);
	}
}

void OIPComms::process_plc_tag_group(const String &tag_group_name) {
	TagGroup &tag_group = tag_groups[tag_group_name];
	String group_tag_path = "protocol=" + tag_group.protocol + "&gateway=" + tag_group.gateway + "&path=" + tag_group.path + "&cpu=" + tag_group.cpu + "&elem_count=";
	for (auto &x : tag_group.plc_tags) {
		const String tag_name = x.first;
		PlcTag &tag = x.second;

		// tag is not initialized
		if (tag.tag_pointer < 0) {
			String tag_path = group_tag_path + itos(tag.elem_count) + "&name=" + tag_name;

			tag.tag_pointer = plc_tag_create(tag_path.utf8().get_data(), timeout);

			// failed to create tag
			if (tag.tag_pointer < 0) {
				print("Failed to create tag: " + tag_name);
				print("Skipping remainder of tag group: " + tag_group_name);
				break;
			} else {
				if (!process_plc_read(tag, tag_name)) {
					print("Skipping remainder of tag group: " + tag_group_name);
					break;
				} else {
					// if read was successful, the tag read is now clean
					tag.dirty = false;
				}
			}
		} else {
			// tag is already initialized, now read it
			if (!process_plc_read(tag, tag_name)) {
				print("Skipping remainder of tag group: " + tag_group_name);
				break;
			} else {
				// if read was successful, the tag read is now clean
				tag.dirty = false;
			}
		}
	}
}

void OIPComms::process_opc_ua_tag_group(const String &tag_group_name) {
	TagGroup &tag_group = tag_groups[tag_group_name];

	UA_StatusCode ret_val = UA_STATUSCODE_BAD;

	// this will only attempt to generate a new client ONCE
	// even if the operation fails, tag_group.client will not be a nullptr on the next pass
	if (tag_group.client == nullptr) {
		tag_group.client = UA_Client_new();

		UA_ClientConfig *config = UA_Client_getConfig(tag_group.client);
		UA_ClientConfig_setDefault(config);
		//config->logging = nullptr;

		const char *endpoint_URL = tag_group.gateway.utf8().get_data();
		ret_val = UA_Client_connect(tag_group.client, endpoint_URL);
		if (ret_val != UA_STATUSCODE_GOOD) {
			print("OIP Comms: The OPC UA connection failed with status code " + String(UA_StatusCode_name(ret_val)));

			// TBD -> don't delete this here, causes a crash - not sure why entirely
			// this might lead to a tiny memory leak, for the client that fails to connect
			// will be cleaned up the next time the sim is ran
			//UA_Client_delete(tag_group.client);
		}
	}

	// not sure if this check is needed
	if (tag_group.client == nullptr)
		return;

	// ensure that the client is actually connected before proceeding
	UA_StatusCode client_status;
	UA_Client_getState(tag_group.client, nullptr, nullptr, &client_status);
	if (client_status != UA_STATUSCODE_GOOD)
		return;

	for (auto &x : tag_group.opc_ua_tags) {
		const String tag_path = x.first;
		OpcUaTag &tag = x.second;

		if (!tag.initialized) {
			UA_Variant_init(&tag.value);

			tag.node_id = UA_NODEID_STRING_ALLOC((UA_UInt16)tag_group.path.to_int(), tag_path.utf8().get_data());
			tag.initialized = true;
		}

		if (tag.initialized) {
			ret_val = UA_Client_readValueAttribute(tag_group.client, tag.node_id, &(tag.value));
			if (ret_val != UA_STATUSCODE_GOOD) {
				UtilityFunctions::print("OIP Comms: OPC UA failed to read " + tag_path + " with status code " + String(UA_StatusCode_name(ret_val)));
			}
		}
	}
}

bool OIPComms::process_plc_read(const PlcTag &tag, const String &tag_name) {
	int read_result = plc_tag_read(tag.tag_pointer, timeout);
	if (read_result != PLCTAG_STATUS_OK) {
		print("Failed to read tag: " + tag_name);
		return false;
	}
	return true;
}

void OIPComms::process() {
	if (enable_comms && sim_running) {
		uint64_t current_ticks = Time::get_singleton()->get_ticks_usec();
		double delta = (current_ticks - last_ticks) / 1000.0f;
		for (auto &x : tag_groups) {
			const String tag_group_name = x.first;
			TagGroup &tag_group = x.second;

			tag_group.time += delta;
			
			if (tag_group.time >= tag_group.polling_interval) {
				queue_tag_group(tag_group_name);
				emit_signal("tag_group_polled", tag_group_name);
				tag_group.time = 0.0f;
			}
		}
		last_ticks = current_ticks;
	}
}

void OIPComms::print(const Variant &message) {
	if (enable_log) {
		UtilityFunctions::print("OIPComms: " + String(message));
	}
}

// --- GDSCRIPT BOUND FUNCTIONS

void OIPComms::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_tag_group", "tag_group_name", "polling_interval", "protocol", "gateway", "path", "cpu"), &OIPComms::register_tag_group);
	ClassDB::bind_method(D_METHOD("register_tag", "tag_group_name", "tag_name", "elem_count"), &OIPComms::register_tag);

	ClassDB::bind_method(D_METHOD("set_enable_comms", "value"), &OIPComms::set_enable_comms);
	ClassDB::bind_method(D_METHOD("get_enable_comms"), &OIPComms::get_enable_comms);

	ClassDB::bind_method(D_METHOD("set_sim_running", "value"), &OIPComms::set_sim_running);
	ClassDB::bind_method(D_METHOD("get_sim_running"), &OIPComms::get_sim_running);

	ClassDB::bind_method(D_METHOD("set_enable_log", "value"), &OIPComms::set_enable_log);
	ClassDB::bind_method(D_METHOD("get_enable_log"), &OIPComms::get_enable_log);

	ClassDB::bind_method(D_METHOD("read_bit", "tag_group_name", "tag_name"), &OIPComms::read_bit);
	ClassDB::bind_method(D_METHOD("read_uint64", "tag_group_name", "tag_name"), &OIPComms::read_uint64);
	ClassDB::bind_method(D_METHOD("read_int64", "tag_group_name", "tag_name"), &OIPComms::read_int64);
	ClassDB::bind_method(D_METHOD("read_uint32", "tag_group_name", "tag_name"), &OIPComms::read_uint32);
	ClassDB::bind_method(D_METHOD("read_int32", "tag_group_name", "tag_name"), &OIPComms::read_int32);
	ClassDB::bind_method(D_METHOD("read_uint16", "tag_group_name", "tag_name"), &OIPComms::read_uint16);
	ClassDB::bind_method(D_METHOD("read_int16", "tag_group_name", "tag_name"), &OIPComms::read_int16);
	ClassDB::bind_method(D_METHOD("read_uint8", "tag_group_name", "tag_name"), &OIPComms::read_uint8);
	ClassDB::bind_method(D_METHOD("read_int8", "tag_group_name", "tag_name"), &OIPComms::read_int8);
	ClassDB::bind_method(D_METHOD("read_float64", "tag_group_name", "tag_name"), &OIPComms::read_float64);
	ClassDB::bind_method(D_METHOD("read_float32", "tag_group_name", "tag_name"), &OIPComms::read_float32);

	ClassDB::bind_method(D_METHOD("write_bit", "tag_group_name", "tag_name", "value"), &OIPComms::write_bit);
	ClassDB::bind_method(D_METHOD("write_uint64", "tag_group_name", "tag_name", "value"), &OIPComms::write_uint64);
	ClassDB::bind_method(D_METHOD("write_int64", "tag_group_name", "tag_name", "value"), &OIPComms::write_int64);
	ClassDB::bind_method(D_METHOD("write_uint32", "tag_group_name", "tag_name", "value"), &OIPComms::write_uint32);
	ClassDB::bind_method(D_METHOD("write_int32", "tag_group_name", "tag_name", "value"), &OIPComms::write_int32);
	ClassDB::bind_method(D_METHOD("write_uint16", "tag_group_name", "tag_name", "value"), &OIPComms::write_uint16);
	ClassDB::bind_method(D_METHOD("write_int16", "tag_group_name", "tag_name", "value"), &OIPComms::write_int16);
	ClassDB::bind_method(D_METHOD("write_uint8", "tag_group_name", "tag_name", "value"), &OIPComms::write_uint8);
	ClassDB::bind_method(D_METHOD("write_int8", "tag_group_name", "tag_name", "value"), &OIPComms::write_int8);
	ClassDB::bind_method(D_METHOD("write_float64", "tag_group_name", "tag_name", "value"), &OIPComms::write_float64);
	ClassDB::bind_method(D_METHOD("write_float32", "tag_group_name", "tag_name", "value"), &OIPComms::write_float32);

	ADD_SIGNAL(MethodInfo("tag_group_polled", PropertyInfo(Variant::STRING, "tag_group_name")));
}

void OIPComms::register_tag_group(const String p_tag_group_name, const int p_polling_interval, const String p_protocol, const String p_gateway, const String p_path, const String p_cpu) {
	if (tag_groups.find(p_tag_group_name) != tag_groups.end()) {
		print("Tag group [" + p_tag_group_name + "] already exists. Overwriting with new values.");
		cleanup_tag_group(p_tag_group_name);
	}

	TagGroup tag_group = {
		p_polling_interval,
		p_polling_interval * 1.0f, // initialize time to polling time and it should kick an initial read
		p_protocol,

		p_gateway,

		p_path,
		p_cpu,
		std::map<String, PlcTag>(),

		nullptr,
		std::map<String, OpcUaTag>()
	};

	tag_groups[p_tag_group_name] = tag_group;
}

bool OIPComms::register_tag(const String p_tag_group_name, const String p_tag_name, const int p_elem_count) {
	if (tag_groups.find(p_tag_group_name) != tag_groups.end()) {
		// TBD -> possibly need to release memory of an existing tag at this address

		TagGroup &tag_group = tag_groups[p_tag_group_name];
		if (tag_group.protocol == "opc_ua") {
			OpcUaTag tag = { false, UA_NODEID_NULL, { 0 } };
			tag_groups[p_tag_group_name].opc_ua_tags[p_tag_name] = tag;
		} else {
			PlcTag tag = {
				-1,
				p_elem_count
			};
			tag_groups[p_tag_group_name].plc_tags[p_tag_name] = tag;
		}
		return true;
	} else {
		print("Tag group [" + p_tag_group_name + "] does not exist. Check the 'Comms' panel below.");
		return false;
	}
}

void OIPComms::set_enable_comms(bool value) {
	enable_comms = value;
	if (value) {
		print("Communications enabled");
	} else {
		print("Communications disabled");
	}
}

bool OIPComms::get_enable_comms() {
	return enable_comms;
}

void OIPComms::set_sim_running(bool value) {
	sim_running = value;
	if (value) {
		print("Sim running");
	} else {
		print("Sim stopped");
	}
}

bool OIPComms::get_sim_running() {
	return sim_running;
}

void OIPComms::set_enable_log(bool value) {
	enable_log = value;
	if (value) {
		// use UtilityFunctions so we can actually see the disabled message
		UtilityFunctions::print("Logging enabled");
	} else {
		UtilityFunctions::print("Logging disabled");
	}

}

bool OIPComms::get_enable_log() {
	return enable_log;
}

// OIP READ/WRITES
// the reads typically occur on the main thread (separate from processing thread)
// need to be a little more careful with thread safety. don't use pass by reference here, copy values
// writes get queued, so should be fine

#define OIP_READ_FUNC(a, b, c)                                                        \
	b OIPComms::read_##a(const String p_tag_group_name, const String p_tag_name) { \
		if (enable_comms && sim_running) {                                         \
			TagGroup &tag_group = tag_groups[p_tag_group_name];                    \
			if (tag_group.protocol == "opc_ua") {                                  \
				OpcUaTag tag = tag_group.opc_ua_tags[p_tag_name];                  \
				if (tag.initialized && UA_Variant_hasScalarType(&tag.value, &UA_TYPES[UA_TYPES_##c])) { \
					return *(b *)tag.value.data; \
				} else { return -1; } \
			} else {                                                               \
				PlcTag tag = tag_group.plc_tags[p_tag_name];                      \
				int32_t tag_pointer = tag.tag_pointer;                             \
				return plc_tag_get_##a(tag_pointer, 0);                            \
			}                                                                      \
		}                                                                          \
		return -1;                                                                 \
	}

OIP_READ_FUNC(bit, bool, BOOLEAN)
OIP_READ_FUNC(uint64, uint64_t, UINT64)
OIP_READ_FUNC(int64, int64_t, INT64)
OIP_READ_FUNC(uint32, uint32_t, UINT32)
OIP_READ_FUNC(int32, int32_t, INT32)
OIP_READ_FUNC(uint16, uint16_t, UINT16)
OIP_READ_FUNC(int16, int16_t, INT16)
OIP_READ_FUNC(uint8, uint8_t, UINT16)
OIP_READ_FUNC(int8, int8_t, INT16)
OIP_READ_FUNC(float64, double, DOUBLE)
OIP_READ_FUNC(float32, float, FLOAT)

#define OIP_WRITE_FUNC(a, b, c)                                                                                                         \
	void OIPComms::write_##a(const String p_tag_group_name, const String p_tag_name, const b p_value) {                                 \
		if (enable_comms && sim_running) {                                                                                              \
			if (tag_groups[p_tag_group_name].protocol == "opc_ua" && !tag_groups[p_tag_group_name].opc_ua_tags[p_tag_name].initialized) \
				return;                                                                                                                 \
			WriteRequest write_req = {                                                                                                  \
				c,                                                                                                                      \
				p_tag_group_name,                                                                                                       \
				p_tag_name,                                                                                                             \
				p_value                                                                                                                 \
			};                                                                                                                          \
			write_queue.push(write_req);                                                                                                \
			tag_group_queue.push("");                                                                                                   \
		}                                                                                                                               \
	}

OIP_WRITE_FUNC(bit, bool, 0)
OIP_WRITE_FUNC(uint64, uint64_t, 1)
OIP_WRITE_FUNC(int64, int64_t, 2)
OIP_WRITE_FUNC(uint32, uint32_t, 3)
OIP_WRITE_FUNC(int32, int32_t, 4)
OIP_WRITE_FUNC(uint16, uint16_t, 5)
OIP_WRITE_FUNC(int16, int16_t, 6)
OIP_WRITE_FUNC(uint8, uint8_t, 7)
OIP_WRITE_FUNC(int8, int8_t, 8)
OIP_WRITE_FUNC(float64, double, 9)
OIP_WRITE_FUNC(float32, float, 10)

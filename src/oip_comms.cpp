#include "oip_comms.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void OIPComms::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_tag_group", "tag_group_name", "polling_interval", "protocol", "gateway", "path", "cpu"), &OIPComms::register_tag_group);
	ClassDB::bind_method(D_METHOD("register_tag", "tag_group_name", "tag_name", "elem_count"), &OIPComms::register_tag);

	ClassDB::bind_method(D_METHOD("read_bit", "tag_group_name", "tag_name"), &OIPComms::read_bit);
	ClassDB::bind_method(D_METHOD("write_bit", "tag_group_name", "tag_name", "value"), &OIPComms::write_bit);

	ClassDB::bind_method(D_METHOD("set_enable_comms", "value"), &OIPComms::set_enable_comms);
	ClassDB::bind_method(D_METHOD("get_enable_comms"), &OIPComms::get_enable_comms);

	ClassDB::bind_method(D_METHOD("set_sim_running", "value"), &OIPComms::set_sim_running);
	ClassDB::bind_method(D_METHOD("get_sim_running"), &OIPComms::get_sim_running);

	ClassDB::bind_method(D_METHOD("set_enable_log", "value"), &OIPComms::set_enable_log);
	ClassDB::bind_method(D_METHOD("get_enable_log"), &OIPComms::get_enable_log);

	ADD_SIGNAL(MethodInfo("tag_group_polled", PropertyInfo(Variant::STRING, "tag_group_name")));
}

OIPComms::OIPComms() {
	// maybe using RefCounted here instead will make warnings go away?

	print("OIPComms: Read thread start");
	work_thread.instantiate();
	work_thread->start(callable_mp(this, &OIPComms::process_work));

	print("OIPComms: Watchdog thread start");
	watchdog_thread.instantiate();
	watchdog_thread->start(callable_mp(this, &OIPComms::watchdog));
}

OIPComms::~OIPComms() {
	watchdog_thread_running = false;
	work_thread_running = false;
	tag_group_queue.shutdown();

	work_thread->wait_to_finish();
	watchdog_thread->wait_to_finish();
	print("OIPComms: Threads shutdown");
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
				//print("Processing writes (no tag groups to be updated)");
			} else {
				print("Tag group not found: " + tag_group_name);
			}
		}
	}
}

void OIPComms::flush_all_writes() {
	while (!write_queue.empty()) {
		WriteRequest write_req = write_queue.front();
		write_queue.pop();
		process_write(write_req);
	}
}

void OIPComms::flush_one_write() {
	if (!write_queue.empty()) {
		WriteRequest write_req = write_queue.front();
		write_queue.pop();
		process_write(write_req);
	}
}

void OIPComms::process_write(const WriteRequest &write_req) {
	int32_t tag_pointer = tag_groups[write_req.tag_group_name].tags[write_req.tag_name].tag_pointer;
	switch (write_req.instruction) {
		case 0:
			plc_tag_set_bit(tag_pointer, 0, write_req.value);
			break;
		case 1:
			break;
	}
	if (plc_tag_write(tag_pointer, timeout) == PLCTAG_STATUS_OK) {
		tag_groups[write_req.tag_group_name].tags[write_req.tag_name].dirty = true;
	} else {
		print("OIPComms: Failed to write tag: " + write_req.tag_name);
	}
}

void OIPComms::queue_tag_group(const String tag_group_name) {
	tag_group_queue.push(tag_group_name);
}

void OIPComms::process_tag_group(const String tag_group_name) {
	TagGroup tag_group = tag_groups[tag_group_name];

	String group_tag_path = "protocol=" + tag_group.protocol + "&gateway=" + tag_group.gateway + "&path=" + tag_group.path + "&cpu=" + tag_group.cpu + "&elem_count=";
	//print("Process tag group " + tag_group_name);
	for (auto const &x : tag_group.tags) {
		String tag_name = x.first;
		Tag tag = x.second;

		// tag is not initialized
		if (tag.tag_pointer < 0) {
			String tag_path = group_tag_path + itos(tag.elem_count) + "&name=" + tag_name;
			tag.tag_pointer = tag_groups[tag_group_name].tags[tag_name].tag_pointer = plc_tag_create(tag_path.utf8().get_data(), timeout);

			// failed to create tag
			if (tag.tag_pointer < 0) {
				print("OIPComms: Failed to create tag: " + tag_name);
				print("OIPComms: Skipping remainder of tag group: " + tag_group_name);
				break;
			} else {
				if (!process_read(tag, tag_name)) {
					print("OIPComms: Skipping remainder of tag group: " + tag_group_name);
					break;
				} else {
					// if read was successful, the tag read is now clean
					tag_groups[tag_group_name].tags[tag_name].dirty = false;
				}
			}
		} else {
			// tag is already initialized, now read it
			if (!process_read(tag, tag_name)) {
				print("OIPComms: Skipping remainder of tag group: " + tag_group_name);
				break;
			} else {
				// if read was successful, the tag read is now clean
				tag_groups[tag_group_name].tags[tag_name].dirty = false;
			}
		}
	}
}

bool OIPComms::process_read(const Tag &tag, const String tag_name) {
	int read_result = plc_tag_read(tag.tag_pointer, timeout);
	if (read_result != PLCTAG_STATUS_OK) {
		print("OIPComms: Failed to read tag: " + tag_name);
		return false;
	}
	return true;
}

void OIPComms::register_tag_group(const String p_tag_group_name, const int p_polling_interval, const String p_protocol, const String p_gateway, const String p_path, const String p_cpu) {
	if (tag_groups.find(p_tag_group_name) != tag_groups.end()) {
		print("OIPComms: Tag group [" + p_tag_group_name + "] already exists. Overwriting with new values.");
	}

	TagGroup tag_group = {
		p_polling_interval,
		p_polling_interval * 1.0f, // initialize time to polling time and it should kick an initial read
		p_protocol,
		p_gateway,
		p_path,
		p_cpu,
		std::map<String, Tag>()
	};

	tag_groups[p_tag_group_name] = tag_group;
}

bool OIPComms::register_tag(const String p_tag_group_name, const String p_tag_name, const int p_elem_count) {
	if (tag_groups.find(p_tag_group_name) != tag_groups.end()) {
		Tag tag = {
			-1,
			p_elem_count
		};
		tag_groups[p_tag_group_name].tags[p_tag_name] = tag;
		return true;
	} else {
		print("OIPComms: Tag group [" + p_tag_group_name + "] does not exist. Check the 'Comms' panel below.");
		return false;
	}
}

int OIPComms::read_bit(const String p_tag_group_name, const String p_tag_name) {
	if (enable_comms && sim_running) {
		Tag tag = tag_groups[p_tag_group_name].tags[p_tag_name];
		if (tag.dirty) {
			// should not do this here - cross threading issues
			//process_read(tag, p_tag_name);

			// right now it's not a huge deal if the tag is dirty. data is in the buffer from the previous write
			// only risk is if another system is writing to the tag, then this read could be incorrect
		}

		int32_t tag_pointer = tag.tag_pointer;
		return plc_tag_get_bit(tag_pointer, 0);
	}
	return -1;
}

void OIPComms::write_bit(const String p_tag_group_name, const String p_tag_name, const int p_value) {
	if (enable_comms && sim_running) {
		WriteRequest write_req = {
			0,
			p_tag_group_name,
			p_tag_name,
			p_value
		};
		write_queue.push(write_req);

		// a little ad hoc - but this forces all writes to flush prior to reading
		tag_group_queue.push("");
	}
}

void OIPComms::process() {
	if (enable_comms && sim_running) {
		uint64_t current_ticks = Time::get_singleton()->get_ticks_usec();
		double delta = (current_ticks - last_ticks) / 1000.0f;
		for (auto const &x : tag_groups) {
			String tag_group_name = x.first;
			tag_groups[tag_group_name].time += delta;

			if (tag_groups[tag_group_name].time >= tag_groups[tag_group_name].polling_interval) {
				queue_tag_group(tag_group_name);
				emit_signal("tag_group_polled", tag_group_name);
				tag_groups[tag_group_name].time = 0.0f;
			}
		}
		last_ticks = current_ticks;
	}
}

void OIPComms::print(const Variant &message) {
	if (enable_log) {
		UtilityFunctions::print(message);
	}
}

// --- GETTERS/SETTERS
void OIPComms::set_enable_comms(bool value) {
	enable_comms = value;
	if (value) {
		print("OIPComms: Communications enabled");
	} else {
		print("OIPComms: Communications disabled");
	}
}

bool OIPComms::get_enable_comms() {
	return enable_comms;
}

void OIPComms::set_sim_running(bool value) {
	sim_running = value;
	if (value) {
		print("OIPComms: Sim running");
	} else {
		print("OIPComms: Sim stopped");
	}
}

bool OIPComms::get_sim_running() {
	return sim_running;
}

void OIPComms::set_enable_log(bool value) {
	enable_log = value;
	if (value) {
		// use UtilityFunctions so we can actually see the disabled message
		UtilityFunctions::print("OIPComms: Logging enabled");
	} else {
		UtilityFunctions::print("OIPComms: Logging disabled");
	}

}

bool OIPComms::get_enable_log() {
	return enable_log;
}

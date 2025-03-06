#include "oip_comms.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>

using namespace godot;

void OIPComms::_bind_methods() {
	ClassDB::bind_method(D_METHOD("comm_test"), &OIPComms::comm_test);

	ClassDB::bind_method(D_METHOD("register_tag_group", "tag_group_name", "polling_interval", "protocol", "gateway", "path", "cpu"), &OIPComms::register_tag_group);
	ClassDB::bind_method(D_METHOD("register_tag", "tag_group_name", "tag_name", "elem_count"), &OIPComms::register_tag);

	ClassDB::bind_method(D_METHOD("read_bit", "tag_group_name", "tag_name"), &OIPComms::read_bit);
	ClassDB::bind_method(D_METHOD("write_bit", "tag_group_name", "tag_name", "value"), &OIPComms::write_bit);
}

OIPComms::OIPComms() {
	UtilityFunctions::print("read thread start");
	read_thread = memnew(Thread);
	read_thread->start(callable_mp(this, &OIPComms::read));

	UtilityFunctions::print("watchdog thread start");
	watchdog_thread = memnew(Thread);
	watchdog_thread->start(callable_mp(this, &OIPComms::watchdog));
}

OIPComms::~OIPComms() {
	/* TBD - this throws an error. not sure how to clean up properly since
	* singleton does not respond to _exit_tree
	* probably need to use notification events
	* https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547

	UtilityFunctions::print("thread quit");
	thread->wait_to_finish();
	memdelete(thread);
	thread = nullptr;
	*/
	watchdog_thread_running = false;
	read_thread_running = false;
	tag_group_queue.shutdown();
}

void OIPComms::watchdog() {
	while (watchdog_thread_running) {
		if (!scene_signals_set) {
			SceneTree *main_scene = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
			if (main_scene != nullptr) {
				main_scene->connect("process_frame", callable_mp(this, &OIPComms::process));
				UtilityFunctions::print("Scene signals set");
				scene_signals_set = true;
			}
		}

		OS::get_singleton()->delay_msec(500);
	}
}

void OIPComms::read() {
	while (read_thread_running) {

		// this pop operation is blocking - thread will sleep until a request comes along
		String tag_group_name = tag_group_queue.pop();
		if (tag_group_name.is_empty() && !read_thread_running)
			break;

		if (tag_groups.find(tag_group_name) != tag_groups.end()) {
			//UtilityFunctions::print("Queueing tag group: " + tag_group_name);
			process_tag_group(tag_group_name);
		} else {
			UtilityFunctions::print("Tag group not found: " + tag_group_name);
		}

	}
}

void OIPComms::queue_tag_group(const String tag_group_name) {
	tag_group_queue.push(tag_group_name);
}

void OIPComms::process_tag_group(const String tag_group_name) {
	TagGroup tag_group = tag_groups[tag_group_name];

	String group_tag_path = "protocol=" + tag_group.protocol + "&gateway=" + tag_group.gateway + "&path=" + tag_group.path + "&cpu=" + tag_group.cpu + "&elem_count=";

	int error_count = 0;
	for (auto const &x : tag_group.tags) {
		String tag_name = x.first;
		Tag tag = x.second;

		// tag is not initialized
		if (tag.tag_pointer < 0) {
			String tag_path = group_tag_path + itos(tag.elem_count) + "&name=" + tag_name;
			tag_groups[tag_group_name].tags[tag_name].tag_pointer = tag.tag_pointer = plc_tag_create(tag_path.utf8().get_data(), timeout);

			// failed to create tag
			if (tag.tag_pointer < 0) {
				UtilityFunctions::print("Failed to create tag: " + tag_name);
				UtilityFunctions::print("Skipping remainder of tag group: " + tag_group_name);
				error_count += 1;
				break;
			}
		} else {
			UtilityFunctions::print("Read tag " + tag_name);
			int read_result = plc_tag_read(tag.tag_pointer, timeout);
			if (read_result != PLCTAG_STATUS_OK) {
				UtilityFunctions::print("Failed to read tag: " + tag_name);
				UtilityFunctions::print("Skipping remainder of tag group: " + tag_group_name);
				error_count += 1;
				break;
			}
		}
	}

	//UtilityFunctions::print("Error count: ", error_count);
}

void OIPComms::register_tag_group(const String p_tag_group_name, const int p_polling_interval, const String p_protocol, const String p_gateway, const String p_path, const String p_cpu) {
	if (tag_groups.find(p_tag_group_name) != tag_groups.end()) {
		UtilityFunctions::print("Tag group [" + p_tag_group_name + "] already exists. Overwriting with new values.");
	}

	TagGroup tag_group = {
		p_polling_interval,
		0.0f,
		p_protocol,
		p_gateway,
		p_path,
		p_cpu,
		std::map<String, Tag>()
	};

	tag_groups[p_tag_group_name] = tag_group;

	//UtilityFunctions::print(tag_groups[p_tag_group_name].polling_interval);
}

void OIPComms::register_tag(const String p_tag_group_name, const String p_tag_name, const int p_elem_count) {
	Tag tag = {
		-1,
		p_elem_count
	};
	tag_groups[p_tag_group_name].tags[p_tag_name] = tag;

	//UtilityFunctions::print(tag_groups[p_tag_group_name].tags[p_tag_name].elem_count);
}

void OIPComms::comm_test() {
	UtilityFunctions::print("start test");

	/*
	int32_t tag = 0;
	int rc;

	String tag_path = "protocol=" + protocol + "&gateway=" + gateway + "&path=" + path + "&cpu=" + cpu + "&elem_count=1&name=" + tag_name;
	tag = plc_tag_create(tag_path.utf8().get_data(), timeout);

	if (tag < 0) {
		UtilityFunctions::print("create failed");
		return;
	}

	rc = plc_tag_read(tag, timeout);
	if (rc != PLCTAG_STATUS_OK) {
		plc_tag_destroy(tag);
		UtilityFunctions::print("read failed");
		return;
	}

	int val = plc_tag_get_bit(tag, 0);
	UtilityFunctions::print("tag (" + tag_name + ") is currently " + itos(val));

	UtilityFunctions::print("flipping tag value");
	plc_tag_set_bit(tag, 0, !val);

	val = plc_tag_get_bit(tag, 0);
	UtilityFunctions::print("tag (" + tag_name + ") is currently " + itos(val));

	rc = plc_tag_write(tag, timeout);
	if (rc != PLCTAG_STATUS_OK) {
		UtilityFunctions::print("write failed");
		plc_tag_destroy(tag);
		return;
	}

	plc_tag_destroy(tag);
	*/
	UtilityFunctions::print("finish test");
}

int OIPComms::read_bit(const String p_tag_group_name, const String p_tag_name) {
	int32_t tag_pointer = tag_groups[p_tag_group_name].tags[p_tag_name].tag_pointer;
	return plc_tag_get_bit(tag_pointer, 0);
}

int OIPComms::write_bit(const String p_tag_group_name, const String p_tag_name, const int p_value) {
	int32_t tag_pointer = tag_groups[p_tag_group_name].tags[p_tag_name].tag_pointer;
	plc_tag_set_bit(tag_pointer, 0, p_value);
	return plc_tag_write(tag_pointer, timeout);
}

void OIPComms::process() {
	uint64_t current_ticks = Time::get_singleton()->get_ticks_usec();
	double delta = (current_ticks - last_ticks) / 1000.0f;
	for (auto const &x : tag_groups) {
		String tag_group_name = x.first;
		tag_groups[tag_group_name].time += delta;

		if (tag_groups[tag_group_name].time >= tag_groups[tag_group_name].polling_interval) {
			queue_tag_group(tag_group_name);
			tag_groups[tag_group_name].time = 0.0f;
		}
	}
	last_ticks = current_ticks;
}

#ifndef OIP_COMMS_H
#define OIP_COMMS_H

#include "libplctag.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <map>
#include <string>
#include <vector>
#include <queue>

#include "oip_blocking_queue.h"

namespace godot {

class OIPComms : public Node {
	GDCLASS(OIPComms, Node)

private:
	int timeout = 5000;

	struct Tag {
		int32_t tag_pointer;
		int elem_count;
		bool dirty;
	};

	struct TagGroup {
		int polling_interval;
		double time;
		String protocol;
		String gateway;
		String path;
		String cpu;

		std::map<String, Tag> tags;
	};
	std::map<String, TagGroup> tag_groups;

	struct WriteRequest {
		uint8_t instruction;
		String tag_group_name;
		String tag_name;
		int value;
	};
	std::queue<WriteRequest> write_queue;

	Ref<Thread> work_thread;
	bool work_thread_running = true;

	Ref<Thread> watchdog_thread;
	bool watchdog_thread_running = true;

	OIPBlockingQueue tag_group_queue;

	uint64_t last_ticks = 0;

	bool scene_signals_set = false;

	bool enable_comms = true;
	bool sim_running = false;

	bool enable_log = false;

	void watchdog();
	void process_work();

	void process_tag_group(const String tag_group_name);
	void queue_tag_group(const String tag_group_name);

	void flush_all_writes();
	void flush_one_write();
	void process_write(const WriteRequest& write_req);
	bool process_read(const Tag &tag, const String tag_name);

	void print(const Variant &message);

protected:
	static void _bind_methods();

public:

	bool get_enable_comms();
	void set_enable_comms(bool value);

	bool get_sim_running();
	void set_sim_running(bool value);

	bool get_enable_log();
	void set_enable_log(bool value);

	void register_tag_group(const String p_tag_group_name, const int p_polling_interval, const String p_protocol, const String p_gateway, const String p_path, const String p_cpu);
	bool register_tag(const String p_tag_group_name, const String p_tag_name, const int p_elem_count);
	int read_bit(const String p_tag_group_name, const String p_tag_name);
	void write_bit(const String p_tag_group_name, const String p_tag_name, const int p_value);

	void process();

	OIPComms();
	~OIPComms();

};

} //namespace godot

#endif

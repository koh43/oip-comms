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

	Thread *work_thread = nullptr;
	bool work_thread_running = true;

	Thread *watchdog_thread = nullptr;
	bool watchdog_thread_running = true;

	OIPBlockingQueue tag_group_queue;

	uint64_t last_ticks = 0;

	bool scene_signals_set = false;

	void watchdog();
	void process_work();

	void process_tag_group(const String tag_group_name);
	void queue_tag_group(const String tag_group_name);

	void flush_all_writes();
	void flush_one_write();
	void process_write(const WriteRequest& write_req);
	bool process_read(const Tag &tag, const String tag_name);


protected:
	static void _bind_methods();

public:

	void comm_test();

	void register_tag_group(const String p_tag_group_name, const int p_polling_interval, const String p_protocol, const String p_gateway, const String p_path, const String p_cpu);

	void register_tag(const String p_tag_group_name, const String p_tag_name, const int p_elem_count);

	int read_bit(const String p_tag_group_name, const String p_tag_name);
	void write_bit(const String p_tag_group_name, const String p_tag_name, const int p_value);

	void process();

	OIPComms();
	~OIPComms();

};

} //namespace godot

#endif

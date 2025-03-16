#ifndef OIP_COMMS_H
#define OIP_COMMS_H

#include "libplctag.h"
#include "open62541.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <map>
#include <string>
#include <vector>
#include <queue>

#include "oip_blocking_queue.h"

#define OIP_READ_FUNC(a, b)                                                        \
	a OIPComms::read_##b(const String p_tag_group_name, const String p_tag_name) { \
		if (enable_comms && sim_running) {                                         \
			Tag tag = tag_groups[p_tag_group_name].tags[p_tag_name];               \
			int32_t tag_pointer = tag.tag_pointer;                                 \
			return plc_tag_get_##b(tag_pointer, 0);                                \
		}                                                                          \
		return -1;                                                                 \
	}

#define OIP_WRITE_FUNC(a, b, c)                                                                         \
	void OIPComms::write_##a(const String p_tag_group_name, const String p_tag_name, const b p_value) { \
		if (enable_comms && sim_running) {                                                              \
			WriteRequest write_req = {                                                                  \
				c,                                                                                      \
				p_tag_group_name,                                                                       \
				p_tag_name,                                                                             \
				p_value                                                                                 \
			};                                                                                          \
			write_queue.push(write_req);                                                                \
			tag_group_queue.push("");                                                                   \
		}                                                                                               \
	}

#define OIP_DECLARE_FUNC(a, b)                                            \
	b read_##a(const String p_tag_group_name, const String p_tag_name); \
	void write_##a(const String p_tag_group_name, const String p_tag_name, const b p_value);

namespace godot {

class OIPComms : public Node {
	GDCLASS(OIPComms, Node)

private:
	int timeout = 5000;

	struct Tag {
		int32_t tag_pointer;
		int elem_count;

		// tag becomes dirty when a write happens before the next polled read
		// TBD - in the future expose an API so that "immediate reads" can occur
		// a little tricky with the current blocking queue/thread implementation
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
		Variant value;
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

	/* original definitions
	int read_bit(const String p_tag_group_name, const String p_tag_name);
	void write_bit(const String p_tag_group_name, const String p_tag_name, const int p_value);
	 */

	OIP_DECLARE_FUNC(bit, bool)
	OIP_DECLARE_FUNC(uint64, uint64_t)
	OIP_DECLARE_FUNC(int64, int64_t)
	OIP_DECLARE_FUNC(uint32, uint32_t)
	OIP_DECLARE_FUNC(int32, int32_t)
	OIP_DECLARE_FUNC(uint16, uint16_t)
	OIP_DECLARE_FUNC(int16, int16_t)
	OIP_DECLARE_FUNC(uint8, uint8_t)
	OIP_DECLARE_FUNC(int8, int8_t)
	OIP_DECLARE_FUNC(float64, double)
	OIP_DECLARE_FUNC(float32, float)

	void opc_ua_test();

	void process();

	OIPComms();
	~OIPComms();

};

} //namespace godot

#endif

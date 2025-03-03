#ifndef OIP_COMMS_H
#define OIP_COMMS_H

#include "libplctag.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

class OIPComms : public Node {
	GDCLASS(OIPComms, Node)

private:
	int timeout = 5000;
	String protocol = "ab_eip";
	String gateway = "localhost";
	String path = "1,2";
	String cpu = "ControlLogix";
	String tag_name = "TEST_INPUT";


protected:
	static void _bind_methods();

	void set_protocol(const String p_protocol);
	String get_protocol() const;

	void set_gateway(const String p_gateway);
	String get_gateway() const;

	void set_path(const String p_path);
	String get_path() const;

	void set_cpu(const String p_cpu);
	String get_cpu() const;

	void set_tag_name(const String p_tag_name);
	String get_tag_name() const;

public:

	void comm_test();
	OIPComms();
	~OIPComms();
};

} //namespace godot

#endif

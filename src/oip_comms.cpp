#include "oip_comms.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void OIPComms::_bind_methods() {
	ClassDB::bind_method(D_METHOD("comm_test"), &OIPComms::comm_test);

	ClassDB::bind_method(D_METHOD("get_protocol"), &OIPComms::get_protocol);
	ClassDB::bind_method(D_METHOD("set_protocol", "protocol"), &OIPComms::set_protocol);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "protocol"), "set_protocol", "get_protocol");

	ClassDB::bind_method(D_METHOD("get_gateway"), &OIPComms::get_gateway);
	ClassDB::bind_method(D_METHOD("set_gateway", "gateway"), &OIPComms::set_gateway);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gateway"), "set_gateway", "get_gateway");

	ClassDB::bind_method(D_METHOD("get_path"), &OIPComms::get_path);
	ClassDB::bind_method(D_METHOD("set_path", "path"), &OIPComms::set_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "set_path", "get_path");

	ClassDB::bind_method(D_METHOD("get_cpu"), &OIPComms::get_cpu);
	ClassDB::bind_method(D_METHOD("set_cpu", "cpu"), &OIPComms::set_cpu);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cpu"), "set_cpu", "get_cpu");

	ClassDB::bind_method(D_METHOD("get_tag_name"), &OIPComms::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag_name"), &OIPComms::set_tag_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
}

OIPComms::OIPComms() {
}

OIPComms::~OIPComms() {
}

void OIPComms::set_protocol(const String p_protocol) {
	protocol = p_protocol;
}

String OIPComms::get_protocol() const {
	return protocol;
}

void OIPComms::set_gateway(const String p_gateway) {
	gateway = p_gateway;
}

String OIPComms::get_gateway() const {
	return gateway;
}

void OIPComms::set_path(const String p_path) {
	path = p_path;
}

String OIPComms::get_path() const {
	return path;
}

void OIPComms::set_cpu(const String p_cpu) {
	cpu = p_cpu;
}

String OIPComms::get_cpu() const {
	return cpu;
}

void OIPComms::set_tag_name(const String p_tag_name) {
	tag_name = p_tag_name;
}

String OIPComms::get_tag_name() const {
	return tag_name;
}

void OIPComms::comm_test() {

	UtilityFunctions::print("start test");

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

	UtilityFunctions::print("finish test");
}

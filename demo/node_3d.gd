@tool
extends Node3D

@export var protocol := "ab_eip":
	get: return protocol
	set(value):
		OIPComms.set_protocol(value)
		protocol = value

@export var gateway := "localhost":
	get: return gateway
	set(value):
		OIPComms.set_gateway(value)
		gateway = value

@export var path := "1,2":
	get: return path
	set(value):
		OIPComms.set_path(value)
		path = value

@export var cpu := "ControlLogix":
	get: return cpu
	set(value):
		OIPComms.set_cpu(value)
		cpu = value

@export var tag_name := "TEST_INPUT":
	get: return tag_name
	set(value):
		OIPComms.set_cpu(value)
		tag_name = value

@export var flip_bit := false: set = _flip_bit

func _flip_bit(_value: bool) -> void:
	OIPComms.comm_test()
	flip_bit = _value

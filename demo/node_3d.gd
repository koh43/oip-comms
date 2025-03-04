@tool
extends Node3D

@export var flip_bit := false: set = _flip_bit

func _flip_bit(_value: bool) -> void:
	OIPComms.comm_test()

@export var register_tag_group := false: set = _register_tag_group 
func _register_tag_group(_value: bool) -> void:
	OIPComms.register_tag_group("test", 1000, "ab_eip", "localhost", "1,2", "ControlLogix")

@export var register_tag := false: set = _register_tag
func _register_tag(_value: bool) -> void:
	OIPComms.register_tag("test", "TEST_INPUT", 1)

@export var add_message := false: set = _add_message
func _add_message(_value: bool) -> void:
	OIPComms.add_message("test")

@tool
extends Node3D

@onready var oip_comms: OIPComms = $OIPComms

@export var flip_bit := false: set = _flip_bit

func _flip_bit(_value: bool) -> void:
	oip_comms.comm_test()
	flip_bit = _value

# Open Industry Project - GDextension Component
This component is for the Open Industry Project (OIP). It enables communications with the following two other libraries:
- https://github.com/libplctag/libplctag
- https://github.com/open62541/open62541

See PR https://github.com/open62541/open62541

# Building from Source
Please read Godot's documentation on building from source and GDextension:
- https://docs.godotengine.org/en/stable/contributing/development/compiling/index.html
- https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_cpp_example.html

Build command:
`scons platform=windows`

Debugging does not currently work because the OIP builds on the .NET version of the Godot engine. Other platforms have not been built yet.

This project uses the standard library.

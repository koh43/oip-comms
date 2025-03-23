# Open Industry Project - GDextension Component
This component is for the Open Industry Project (OIP). It enables communications with the following two other libraries:
- https://github.com/libplctag/libplctag
- https://github.com/open62541/open62541

See PR [https://github.com/open62541/open62541](https://github.com/Open-Industry-Project/Open-Industry-Project/pull/161)

# Building from Source
Please read Godot's documentation on building from source and GDextension:
- https://docs.godotengine.org/en/stable/contributing/development/compiling/index.html
- https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_cpp_example.html

Build command:
`scons platform=windows debug_symbols=yes`

The output of building will be the DLLs located in: https://github.com/bikemurt/OIP_gdext/tree/main/demo/bin/windows

The DLLs, and `oip_comms.gdextension` file must be copied to the `oip_comms` dock plugin for the main Open Industry Project repo: `Open-Industry-Project/addons/oip_comms/bin/`. Right now just building for Windows, but should be extendable to other platforms.

This GDextension as well as the libs (libplctag, open62541) are built with the `/MT` flag. According to dumpbin this removes any external deps on MSVC runtime and should improve portability.

https://stackoverflow.com/a/56061183/7132687

This project uses the standard library.

# Debugging
As long as you build with `debug_symbols=yes`, the 4.5 branch of OIP will be able to debug this GDextension application. 

Inside the `.vs/` folder you can create `launch.vs.json`:

```
{
	"version": "0.2.1",
	"defaults": {},
	"configurations": [
		{
			"type": "default",
			"project": "location_of_oip_4.5_build\\godot.windows.editor.x86_64.exe",
			"name": "Godot Editor",
			"args": [ "location_of_project\\Open-Industry-Project\\project.godot" ]
		}
	]
}
```

Then you can launch the OIP editor from Visual Studio and drop in breakpoints to test.

# Licensing
Right now technically there is no license and use is only as a part of the Open Industry Project.
I haven't gone through licensing requirements yet, but please review licensing requirements of the Godot Engine, the Open Industry Project, libplctag and open62541. It's going to be the common denominator of those licenses.

# Vector Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Vector is a powerful, free and open-source community-driven 2D and 3D game engine. This repository hosts a version of the engine based on Godot Engine.

## About The Project

Vector aims to provide a feature-rich, multi-platform game engine with an intuitive, integrated development environment. It allows developers to create games from simple 2D projects to complex 3D experiences.

Key features inherited and built upon include:

*   **Intuitive Scene-Based Design:** Create games using a flexible scene system.
*   **Versatile Scripting:** Use GDScript, a Python-like scripting language, or C# with .NET support, and C++ via GDNative/GDExtension.
*   **Powerful 2D & 3D Rendering:** High-quality rendering capabilities for both 2D and 3D, with support for various post-processing effects, shaders, and more.
*   **Integrated Editor:** A feature-packed editor with tools for scripting, animation, tilemaps, visual shaders, and more.
*   **Multi-Platform Export:** Deploy your games to a wide range of platforms including Windows, macOS, Linux, Android, iOS, and Web.
*   **Animation Tools:** A robust animation system for characters, UI elements, and in-game objects.
*   **Physics Engine:** Built-in 2D and 3D physics engines.
*   **Extensible Architecture:** Extend the engine's capabilities with C++ modules or GDExtensions.
*   **Open Source:** Freely use, modify, and distribute the engine under the MIT license.

This specific repository, `vector_ai`, is maintained by @improdead.

## Getting Started

As Vector is based on the Godot Engine, the process for getting started, building, and contributing is similar.

### Prerequisites

To compile Vector from source, you will generally need:

*   Python (version 3.6 or later)
*   SCons (version 3.1.2 or later)
*   A C++ compiler (e.g., GCC, Clang, MSVC)
*   Platform-specific SDKs if you plan to export to those platforms (e.g., Android SDK, Xcode).

### Building from Source

The compilation process is handled by SCons. Detailed instructions for various platforms can be found in the official Godot Engine documentation:

*   [Compiling for Windows](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_windows.html)
*   [Compiling for macOS](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_macos.html)
*   [Compiling for Linux, *BSD](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_x11.html)
*   [Compiling for Android](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_android.html)
*   [Compiling for iOS](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_ios.html)
*   [Compiling for Web](https://docs.godotengine.org/en/stable/development/compiling/compiling_for_web.html)

Adapt the commands provided in the Godot documentation to this `Vector` source code repository. A general command to compile the editor for your current platform might look like:

```bash
scons platform=<your_platform> -j<number_of_cores>
```

Replace `<your_platform>` with `windows`, `linuxbsd` (for X11), `macos`, etc., and `<number_of_cores>` with the number of CPU cores you want to use for compilation.

### Running the Editor

Once successfully compiled, the editor executable will typically be found in the `bin/` subdirectory (e.g., `bin/godot.windows.editor.x86_64.exe` on Windows).

## Project Structure

The engine's source code is organized into several main directories:

*   `core/`: Core engine functionalities, data types, and servers.
*   `scene/`: Scene system, nodes, and UI elements.
*   `editor/`: The editor application and its tools.
*   `drivers/`: Hardware and platform-specific drivers (rendering, audio, etc.).
*   `modules/`: Optional engine modules that provide additional features.
*   `platform/`: Platform-specific code for porting the engine.
*   `main/`: Main entry point and OS-level interaction.
*   `doc/`: Documentation files.
*   `thirdparty/`: External libraries used by the engine.

## Contributing

Since this is currently a personal repository, contributions are at the discretion of the maintainer (@improdead). If you are interested in contributing, please check with the repository owner.

For general contributions to the upstream Godot Engine, please refer to their [contribution guidelines](https://docs.godotengine.org/en/stable/community/contributing/index.html).

## License

Vector Engine (this fork) is distributed under the MIT license, which can be found in the `LICENSE` file.

Copyright (c) 2025 Dekai Li.

This project is based on the Godot Engine, also distributed under the MIT license.
Copyright (c) 2014-present Godot Engine contributors.
Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.

## Acknowledgements

*   The Godot Engine team and its vibrant community for creating and maintaining an exceptional open-source game engine.

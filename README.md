# SpookyLights

SpookyLights is a 2D light-transport renderer I created in C/C++. It simulates rays moving through scenes containing mirrors, scattering surfaces, refractive objects, coloured materials, and black holes.

The renderer supports features including:

- reflection, refraction, and Fresnel reflection
- colour filtering and light absorption
- wavelength-based dispersion
- black-hole gravitational lensing
- recursive and stepwise ray propagation
- an interactive OpenGL/GLUT GUI
- movie-mode ray propagation with configurable speed, physics resolution, and fading trails

Scenes are described with simple text files, and the renderer can be run either from the command line or through the GUI.

## Sample scenes

The repository includes several sample scene files that demonstrate different parts of the renderer, including refraction and dispersion, coloured materials, black-hole lensing, movie-mode propagation, and the lotus showcase scene. These are a good starting point for exploring the renderer or testing different command-line options.

## Building

Command-line version:

```bash
sh compile.sh
```

GUI version:

```bash
sh compile_GUI.sh
```

See `usage.txt` for the full set of command-line options and scene examples.

## Credits

SpookyLights was created and developed by me.

The project builds on starter code originally provided by Francisco Estrada. In particular, the OpenGL/GLUT windowing code and the image-output/display infrastructure are based on his original implementation and remain credited to him.

The renderer features, extensions, scene work, and project-specific implementation in this repository were developed by me.

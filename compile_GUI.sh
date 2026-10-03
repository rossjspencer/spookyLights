# build the gui renderer and enable its OpenGL code
g++ -O3 -fopenmp -DUSE_GL spookyLights.c -lm -lglut -lGL -lGLU -lX11 -o spookyLights

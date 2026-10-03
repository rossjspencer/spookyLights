#ifndef SPOOKY_LIGHTS_H
#define SPOOKY_LIGHTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef USE_GL

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#define PI 3.1415926535
#define TOL .000001
#define MAX_OBJECTS 1000
#define MAX_HOLES 10 // maximum number of black holes that can be added to a scene
#define MAX_LIGHTS 1000
#define DEFAULT_PHYSICS_STEP_LENGTH 0.01 // default distance a ray moves before physics is recalculated
#define DEFAULT_TIME_BETWEEN_FRAMES 0.05 // default simulated time between movie frames
#define DEFAULT_MOVIE_RAY_SPEED 1.0 // default scene distance travelled per unit of movie time
#define DEFAULT_MOVIE_TRAIL_FADE 0.0 // default movie trail fade, 0 disables the trail effect
#define MOVIE_TRAIL_VISIBILITY_TOLERANCE 0.005 // once the faded movie trail drops below this, we consider it gone
#define DEFAULT_MAX_RAY_LENGTH 100.0 // default total distance a stepwise/movie ray is allowed to travel
#define MAX_QUEUE 10000 // maximum number of rays that can be active in movie mode
#define GUI_RAYS_PER_FRAME 10 // number of new rays added per gui frame outside movie mode

struct point2D
{
	double px;
	double py;
};

struct ray2D
{
	struct point2D p;
	struct point2D d;
	double red, green, blue;
	int insideOut;

	int monochromatic;
	double hue;
};

// stores the state we need to keep for an active movie ray
struct queuedRay
{
	struct ray2D ray;
	int depth;
	double distanceTravelled; // total distance this ray has travelled
	double physicsDistanceRemaining; // distance left before the next physics update
};

// fixed size circular queue used by movie propagation
struct rayQueue
{
	struct queuedRay rays[MAX_QUEUE];
	int front;
	int back;
	int count;
};

struct circ2D
{
	struct point2D c;
	double r;
	int materialType;

	double refractiveIndex;
	double red, green, blue;
};

struct wall2D
{
	struct ray2D w;
	int materialType;
	double red, green, blue;
};

struct light2D
{
	struct ray2D l;

	int lightType;

	double red, green, blue;
};

// holds the information needed for one black hole
struct blackHole
{
	struct point2D c; // center location
	double mass; // mass of the black hole (higher mass bends more)
	double eventHorizon;
};

double *imRGB, *iT;
unsigned char *im;
int sx, sy;
int maxDepth;
int numRays;
struct circ2D objects[MAX_OBJECTS];

struct blackHole blackHoles[MAX_HOLES]; // array of black holes that can bend nearby rays
int numBlackHoles = 0;
struct wall2D walls[4];
struct light2D lss[MAX_LIGHTS];
struct light2D lightSource;
int numLights = 0;
double wX, wY;
double cutoffHigh, cutoffLow;
double dispersion = 0;
double spectrum[2048 * 3];
char sceneName[1024];
int doSteps = 0; // whether the program should do stepwise calculations of photon trajectories
int movieMode = 0; // whether rays should advance by a time interval per gui frame
double movieTime = 0.0; // current simulation time in movie mode
double timeBetweenFrames = DEFAULT_TIME_BETWEEN_FRAMES; // simulated time between movie frames
double movieRaySpeed = DEFAULT_MOVIE_RAY_SPEED; // scene distance travelled per unit of movie time
double movieTrailFade = DEFAULT_MOVIE_TRAIL_FADE; // movie only display fade for leaving a visible trail behind moving photons
int renderObjectsEnabled = 0; // whether object outlines are rendered
int guiRayCount = 120; // total number of initial rays traced per gui render
double physicsStepLength = DEFAULT_PHYSICS_STEP_LENGTH; // maximum distance a ray moves before physics is recalculated
double maxRayLength = DEFAULT_MAX_RAY_LENGTH; // the maximum distance a stepwise ray branch can travel
double scatterRetention = 0.7; // fraction of ray energy kept after a scatter reflection, default 0.7
double minRayIntensity = 0.0; // rays below this maximum RGB component are stopped, 0 disables the cutoff
int doWallCollision = 1; // whether light should bounce off the walls (on by default)
#ifdef USE_GL
int windowID;
int movieStarted = 0; // movie mode waits for enter before the photon packet starts moving
int mouseState = 0;
int mouseButton = 0;
int dragObjectType;
int dragObjectIndex = -1;
double lightDistThresh = .25;
double prevX, prevY;
#endif

void hue2RGB(double hue, double *red, double *green, double *blue);
void renderObjects(void);

void renderRay(struct point2D *p1, struct point2D *p2, double red, double green, double blue);
void setPixel(double x, double y, double red, double green, double blue);

double dot(struct point2D *p, struct point2D *q);
void normalize(struct point2D *d);

void addCirc(struct point2D *c, double r, int type, double n, double red, double green, double blue);
void addBlackHole(struct point2D *c, double mass); // adds a black hole at the given position and mass
void buildWalls(void);
void buildScene(void);

#ifdef USE_GL

void initGlut(const char *winName, int sizeX, int sizeY, int positionX, int positionY);

void WindowReshape(int w, int h);
void WindowDisplay(void);
void kbHandler(unsigned char key, int x, int y);
void mouseClickHandler(int button, int state, int x, int y);
void mouseDragHandler(int x, int y);
#endif

#endif

// USE_GL is enabled by compile_GUI.sh when the gui build is requested
// #define USE_GL

// #define __DEBUG_MODE

#include "spookyLights.h"
#include "buildScene.c"

#include "rays2D.h"
#include "rays2D.c"

#ifdef USE_GL

// keeps track of how many initial rays have already been created across gui frames
static int guiRaysGenerated = 0;

#endif

inline void hue2RGB(double hue, double *red, double *green, double *blue)
{
	FILE *f;
	static int read = 0;
	char line[1024];
	static unsigned char *rt = NULL;

	if (read == 0)
	{
		rt = (unsigned char *)calloc(700 * 72 * 3, sizeof(unsigned char));
		f = fopen("rainbow.ppm", "r");
		if (f == NULL)
		{
			fprintf(stderr, "Unable to open rainbow.ppm\n");
			return;
		}
		fgets(&line[0], 1023, f);
		fgets(&line[0], 1023, f);
		fgets(&line[0], 1023, f);
		fgets(&line[0], 1023, f);
		fread(rt, 700 * 72 * 3 * sizeof(unsigned char), 1, f);
		fclose(f);
		read = 1;
	}

	hue = (1.0 - hue) * 700;
	if (hue < 0) hue = 0;
	if (hue > 699) hue = 699;

	*red = (double)(*(rt + (((int)hue) * 3) + 0)) / 255.0;
	*green = (double)(*(rt + (((int)hue) * 3) + 1)) / 255.0;
	*blue = (double)(*(rt + (((int)hue) * 3) + 2)) / 255.0;

	return;
}

// draws optional object outlines after the ray image has been converted for display
void renderObjects(void)
{
	double x, y;
	int xx, yy;

	// render regular objects in their material colour
	for (int i = 0; i < MAX_OBJECTS; i++)
	{
		if (objects[i].r <= 0) break;

		for (double ang = 0; ang < 2 * PI; ang += .001)
		{
			x = objects[i].c.px + (cos(ang) * objects[i].r);
			y = objects[i].c.py + (sin(ang) * objects[i].r);

			x -= W_LEFT;
			y -= W_TOP;
			x = x / (W_RIGHT - W_LEFT);
			y = y / (W_BOTTOM - W_TOP);
			x = x * (sx - 1);
			y = y * (sy - 1);

			xx = (int)round(x);
			yy = (int)round(y);

			if (xx >= 0 && xx < sx && yy >= 0 && yy < sy)
			{
				*(im + ((xx + (yy * sx)) * 3) + 0) = (unsigned char)(255.0 * objects[i].red);
				*(im + ((xx + (yy * sx)) * 3) + 1) = (unsigned char)(255.0 * objects[i].green);
				*(im + ((xx + (yy * sx)) * 3) + 2) = (unsigned char)(255.0 * objects[i].blue);
			}
		}
	}

	// render black hole event horizons in purple
	for (int i = 0; i < numBlackHoles; i++)
	{
		for (double ang = 0; ang < 2 * PI; ang += .001)
		{
			x = blackHoles[i].c.px + (cos(ang) * blackHoles[i].eventHorizon);
			y = blackHoles[i].c.py + (sin(ang) * blackHoles[i].eventHorizon);

			x -= W_LEFT;
			y -= W_TOP;
			x = x / (W_RIGHT - W_LEFT);
			y = y / (W_BOTTOM - W_TOP);
			x = x * (sx - 1);
			y = y * (sy - 1);

			xx = (int)round(x);
			yy = (int)round(y);

			if (xx >= 0 && xx < sx && yy >= 0 && yy < sy)
			{
				*(im + ((xx + (yy * sx)) * 3) + 0) = 255;
				*(im + ((xx + (yy * sx)) * 3) + 1) = 0;
				*(im + ((xx + (yy * sx)) * 3) + 2) = 255;
			}
		}
	}
}

inline void renderRay(struct point2D *p1, struct point2D *p2, double red, double green, double blue)
{

	double x1, y1, x2, y2, xt, yt;
	int xx, yy;
	double dx, dy;
	double inc;
	double pixIncX, pixIncY;

	pixIncX = 1.0 * (W_RIGHT - W_LEFT) / sx;
	pixIncY = 1.0 * (W_TOP - W_BOTTOM) / sy;

	if (p1->px < W_LEFT - TOL || p1->px > W_RIGHT + TOL || p1->py < W_TOP - TOL || p1->py > W_BOTTOM + TOL ||
		p2->px < W_LEFT - TOL || p2->px > W_RIGHT + TOL || p2->py < W_TOP - TOL || p2->py > W_BOTTOM + TOL)
	{
#ifndef __QUIET
		fprintf(stderr, "renderRay() - at least one endpoint is outside the image bounds, somewhere there's an error...\n");
		fprintf(stderr, "p1=(%f,%f)\n", p1->px, p1->py);
		fprintf(stderr, "p2=(%f,%f)\n", p2->px, p2->py);
#endif
		return;
	}

	x1 = p1->px - W_LEFT + (pixIncX * (drand48() - .5));
	y1 = p1->py - W_TOP + (pixIncY * (drand48() - .5));
	x2 = p2->px - W_LEFT + (pixIncX * (drand48() - .5));
	y2 = p2->py - W_TOP + (pixIncY * (drand48() - .5));

	x1 = x1 / (W_RIGHT - W_LEFT);
	y1 = y1 / (W_BOTTOM - W_TOP);
	x2 = x2 / (W_RIGHT - W_LEFT);
	y2 = y2 / (W_BOTTOM - W_TOP);

	x1 = x1 * (sx - 1);
	y1 = y1 * (sy - 1);
	x2 = x2 * (sx - 1);
	y2 = y2 * (sy - 1);

	dx = x2 - x1;
	dy = y2 - y1;

	if (abs(dx) >= abs(dy))
	{
		if (x2 < x1)
		{
			xt = x1;
			yt = y1;
			x1 = x2;
			y1 = y2;
			x2 = xt;
			y2 = yt;
		}

		yt = y1;
		inc = (y2 - y1) / abs(x2 - x1);
		for (double xt = x1; xt <= x2; xt += 1)
		{
			xx = (int)round(xt);
			yy = (int)round(yt);
			if (xx >= 0 && xx < sx && yy >= 0 && yy < sy)
			{
				(*(imRGB + ((xx + (yy * sx)) * 3) + 0)) += red;
				(*(imRGB + ((xx + (yy * sx)) * 3) + 1)) += green;
				(*(imRGB + ((xx + (yy * sx)) * 3) + 2)) += blue;
			}
			yt += inc;
		}
	}
	else
	{
		if (y2 < y1)
		{
			xt = x1;
			yt = y1;
			x1 = x2;
			y1 = y2;
			x2 = xt;
			y2 = yt;
		}

		xt = x1;
		inc = (x2 - x1) / abs(y2 - y1);
		for (double yt = y1; yt <= y2; yt += 1)
		{
			xx = (int)round(xt);
			yy = (int)round(yt);
			if (xx >= 0 && xx < sx && yy >= 0 && yy < sy)
			{
				(*(imRGB + ((xx + (yy * sx)) * 3) + 0)) += red;
				(*(imRGB + ((xx + (yy * sx)) * 3) + 1)) += green;
				(*(imRGB + ((xx + (yy * sx)) * 3) + 2)) += blue;
			}
			xt += inc;
		}
	}
}

inline void setPixel(double x, double y, double red, double green, double blue)
{

	int xx, yy;
	int ii, jj;
	double weight;

	if (red < 0 || green < 0 || blue < 0 || red > 1 || green > 1 || blue > 1)
		fprintf(stderr, "Invalid RGB colours passed to setPixel() - image will have artifacts!\n");

	x -= W_LEFT;
	y -= W_TOP;
	x = x / (W_RIGHT - W_LEFT);
	y = y / (W_BOTTOM - W_TOP);
	x = x * (sx - 1);
	y = y * (sy - 1);

	xx = (int)round(x);
	yy = (int)round(y);

	for (int i = xx - 1; i <= xx + 1; i++)
		for (int j = yy - 1; j <= yy + 1; j++)
		{
			weight = exp(-(((x - i) * (x - i)) + ((y - j) * (y - j))) * .5);
			if (i >= 0 && j >= 0 && i < sx && j < sy)
			{
				(*(imRGB + ((i + (j * sx)) * 3) + 0)) += weight * red;
				(*(imRGB + ((i + (j * sx)) * 3) + 1)) += weight * green;
				(*(imRGB + ((i + (j * sx)) * 3) + 2)) += weight * blue;
			}
		}
}

inline double dot(struct point2D *p, struct point2D *q)
{
	return ((p->px * q->px) + (p->py * q->py));
}

inline void normalize(struct point2D *d)
{
	double l;
	l = d->px * d->px;
	l += (d->py * d->py);
	if (l > 0)
	{
		l = sqrt(l);
		d->px = d->px / l;
		d->py = d->py / l;
	}
}

void addCirc(struct point2D *c, double r, int type, double refractiveIndex, double red, double green, double blue)
{

	static int numObjects = 0;

	if (numObjects >= MAX_OBJECTS)
	{
		fprintf(stderr, "List of objects is full!\n");
		return;
	}
	objects[numObjects].c = *c;
	objects[numObjects].r = r;
	objects[numObjects].materialType = type;
	objects[numObjects].refractiveIndex = refractiveIndex;
	objects[numObjects].red = red;
	objects[numObjects].green = green;
	objects[numObjects].blue = blue;
	numObjects++;
}

// adds a black hole and automatically enables the stepwise calculations needed for gravity
void addBlackHole(struct point2D *c, double mass)
{
	// add a black hole to the next available slot
	if (numBlackHoles >= MAX_HOLES)
	{
		fprintf(stderr, "No more black holes for you, mister.\n");
		return;
	}

	doSteps = 1; // black holes require stepwise propagation
	blackHoles[numBlackHoles].c = *c;
	blackHoles[numBlackHoles].mass = mass;
	// schwarzschild radius, anything closer has reached the event horizon
	blackHoles[numBlackHoles].eventHorizon = (2.0 * 6.67430e-11 * mass) / (299792458.0 * 299792458.0);
	numBlackHoles++;
}

int main(int argc, char *argv[])
{
	struct ray2D ray;
	struct point2D p, d;
	double mx, mi, rng;
	int hist[256];
	int idx, low, hi, acc;
	int cs, ms;
	double lnorm, hnorm, hrange;
	FILE *f;

	if (argc < 7)
	{
		fprintf(stderr, "USAGE: spookyLights max_depth dispersion cutoff_low cutoff_high num_samples scene_name\n");
		fprintf(stderr, "  max_depth - Maximum recursion depth (in [1 25])\n");
		fprintf(stderr, "  dispersion - value between 0 (disabled) and .5\n");
		fprintf(stderr, "  cutoff_low - histogram equalization threshold for dark regions, try values in 0.01 to 0.1\n");
		fprintf(stderr, "  cutoff_high - histogram equalization threshold for bright regions, try values in 0.01 to 0.1\n");
		fprintf(stderr, "  num_samples - maximum number of samples for command-line mode\n");
		fprintf(stderr, "  scene_name - name of the text file containing the scene description\n");
		fprintf(stderr, "  --objects - render object outlines and black hole event horizons\n");
		fprintf(stderr, "  --steps - force stepwise propagation even without black holes\n");
		fprintf(stderr, "  --movie - use movie propagation mode\n");
		fprintf(stderr, "  --frame-time X - simulated time between movie frames, must be positive\n");
		fprintf(stderr, "  --movie-speed X - scene distance travelled per unit of movie time, must be positive\n");
		fprintf(stderr, "  --physics-step X - maximum distance between physics updates, must be greater than the numerical tolerance\n");
		fprintf(stderr, "  --trail-fade X - movie-mode display fade in [0,1), 0 disables trails\n");
		fprintf(stderr, "  --gui-rays N - set the total number of initial rays traced in the gui\n");
		fprintf(stderr, "  --scatter-retention X - fraction of energy kept after a scatter reflection, in [0,1]\n");
		fprintf(stderr, "  --min-intensity X - stop rays once every RGB channel is below X, in [0,1]\n");
		exit(0);
	}

	sx = 1024;
	sy = 1024;
	numRays = 1000000;
	maxDepth = atoi(argv[1]);
	dispersion = atof(argv[2]);
	cutoffLow = atof(argv[3]);
	cutoffHigh = atof(argv[4]);
	ms = atoi(argv[5]);
	strcpy(&sceneName[0], argv[6]);

	// parse the optional feature flags without changing the original positional arguments
	for (int i = 7; i < argc; i++)
	{
		// object rendering is optional so it does not cover the ray image unless requested
		if (strcmp(argv[i], "--objects") == 0)
		{
			renderObjectsEnabled = 1;
		}
		// force the ordinary stepwise path even if the scene does not contain black holes
		else if (strcmp(argv[i], "--steps") == 0)
		{
			doSteps = 1;
		}
		// movie mode keeps the same rays alive and advances all of them through one time interval per frame
		else if (strcmp(argv[i], "--movie") == 0)
		{
			movieMode = 1;
			doSteps = 1;
		}
		// let the user control how much simulated time passes before the next movie frame
		else if (strcmp(argv[i], "--frame-time") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--frame-time requires a positive value\n");
				exit(0);
			}
			timeBetweenFrames = atof(argv[++i]);
			if (!isfinite(timeBetweenFrames) || timeBetweenFrames <= 0.0)
			{
				fprintf(stderr, "--frame-time requires a positive value\n");
				exit(0);
			}
		}
		// changing the movie speed changes how far the packet travels per unit of simulated time
		else if (strcmp(argv[i], "--movie-speed") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--movie-speed requires a positive value\n");
				exit(0);
			}
			movieRaySpeed = atof(argv[++i]);
			if (!isfinite(movieRaySpeed) || movieRaySpeed <= 0.0)
			{
				fprintf(stderr, "--movie-speed requires a positive value\n");
				exit(0);
			}
		}
		// keep the physics resolution adjustable independently from the visible movie frame length
		else if (strcmp(argv[i], "--physics-step") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--physics-step requires a positive value\n");
				exit(0);
			}
			physicsStepLength = atof(argv[++i]);
			if (!isfinite(physicsStepLength) || physicsStepLength <= TOL)
			{
				fprintf(stderr, "--physics-step must be greater than %g\n", TOL);
				exit(0);
			}
		}
		// let movie mode keep a fading trail behind the photon packet without changing the actual ray energy
		else if (strcmp(argv[i], "--trail-fade") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--trail-fade requires a value in [0,1)\n");
				exit(0);
			}
			movieTrailFade = atof(argv[++i]);
			if (!isfinite(movieTrailFade) || movieTrailFade < 0.0 || movieTrailFade >= 1.0)
			{
				fprintf(stderr, "--trail-fade requires a value in [0,1)\n");
				exit(0);
			}
		}
		// let gui tests choose how many initial rays should be created instead of using a fixed amount
		else if (strcmp(argv[i], "--gui-rays") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--gui-rays requires a positive integer\n");
				exit(0);
			}
			guiRayCount = atoi(argv[++i]);
			if (guiRayCount <= 0)
			{
				fprintf(stderr, "--gui-rays requires a positive integer\n");
				exit(0);
			}
		}
		// let scatter absorption be tuned without recompiling
		else if (strcmp(argv[i], "--scatter-retention") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--scatter-retention requires a value in [0,1]\n");
				exit(0);
			}
			scatterRetention = atof(argv[++i]);
			if (!isfinite(scatterRetention) || scatterRetention < 0.0 || scatterRetention > 1.0)
			{
				fprintf(stderr, "--scatter-retention requires a value in [0,1]\n");
				exit(0);
			}
		}
		// optionally stop rays once their remaining colour energy is too small to matter
		else if (strcmp(argv[i], "--min-intensity") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "--min-intensity requires a value in [0,1]\n");
				exit(0);
			}
			minRayIntensity = atof(argv[++i]);
			if (!isfinite(minRayIntensity) || minRayIntensity < 0.0 || minRayIntensity > 1.0)
			{
				fprintf(stderr, "--min-intensity requires a value in [0,1]\n");
				exit(0);
			}
		}
		// rays that reach a wall are allowed to leave instead of reflecting/scattering from it
		else if (strcmp(argv[i], "--noWalls") == 0)
		{
			doWallCollision = 0;
		}
		else
		{
			fprintf(stderr, "Unknown optional argument: %s\n", argv[i]);
			exit(0);
		}
	}

	// movie mode keeps all gui rays in the queue at the same time
	if (movieMode && guiRayCount > MAX_QUEUE)
	{
		fprintf(stderr, "--gui-rays cannot exceed %d in movie mode\n", MAX_QUEUE);
		exit(0);
	}

	// movie frames need to advance by more than the numerical tolerance or the packet would never move
	if (movieMode)
	{
		double distancePerFrame = movieRaySpeed * timeBetweenFrames;
		if (!isfinite(distancePerFrame) || distancePerFrame <= TOL)
		{
			fprintf(stderr, "movieRaySpeed * timeBetweenFrames must be greater than %g\n", TOL);
			exit(0);
		}
	}

	fprintf(stderr, "Working with:\n");
	fprintf(stderr, "Image size (%d, %d)\n", sx, sy);
	fprintf(stderr, "Max. recursion depth: %d\n", maxDepth);
	fprintf(stderr, "Dispersion=%f\n", dispersion);
	fprintf(stderr, "Cutoff low=%f\n", cutoffLow);
	fprintf(stderr, "Cutoff high=%f\n", cutoffHigh);
	fprintf(stderr, "Num samples=%d\n", ms);
	fprintf(stderr, "Scene name: %s\n", sceneName);
	fprintf(stderr, "GUI ray count: %d\n", guiRayCount);
	fprintf(stderr, "Scatter retention: %f\n", scatterRetention);
	fprintf(stderr, "Minimum ray intensity: %f\n", minRayIntensity);
	fprintf(stderr, "Movie trail fade: %f\n", movieTrailFade);

	if (maxDepth < 1 || maxDepth > 25 || dispersion < 0 || dispersion > 1.0 || cutoffLow < .01 || cutoffLow > .25 || cutoffHigh < .01 || cutoffHigh > .25 || ms < 1000 || ms > 100000000)
	{
		fprintf(stderr, "Image size must be in [100,5000]\n");
		fprintf(stderr, "Max depth must be in [1,25]\n");
		fprintf(stderr, "Dispersion must be in [0,1.0]\n");
		fprintf(stderr, "Cutoffs must be in [.01, .25]\n");
		fprintf(stderr, "Num samples must be in [1000,100000000]\n");
		exit(0);
	}

	imRGB = (double *)calloc(sx * sy * 3, sizeof(double));
	iT = (double *)calloc(sx * sy * 3, sizeof(double));
	im = (unsigned char *)calloc(sx * sy * 3, sizeof(unsigned char));
	if (imRGB == NULL || iT == NULL || im == NULL)
	{
		fprintf(stderr, "Out of memory?! is this a Commodore 64 you're running on???\n");
		exit(0);
	}

	memset(&objects[0], 0, MAX_OBJECTS * sizeof(struct circ2D));
	// black holes have their own array so regular object parsing/rendering stays unchanged
	memset(&blackHoles[0], 0, MAX_HOLES * sizeof(struct blackHole));
	memset(&lss[0], 0, MAX_LIGHTS * sizeof(struct light2D));
	memset(&walls[0], 0, 4 * sizeof(struct ray2D));
	for (int i = 0; i < MAX_OBJECTS; i++) objects[i].r = -1;

	f = fopen("spectrum.dat", "r");
	if (f == NULL)
	{
		fprintf(stderr, "Unable to load light spectrum data!\n");
		for (int i = 0; i < 2048 * 3; i++) spectrum[i] = 1.0;
	}
	else
	{
		fread(&spectrum[0], 2048 * 3 * sizeof(double), 1, f);
		fclose(f);
	}

	buildWalls();
	buildScene();
	initializeSourceSpectra();

	wX = (W_RIGHT - W_LEFT);
	wY = (W_BOTTOM - W_TOP);

#ifdef USE_GL

	glutInit(&argc, argv);
	initGlut("SpookyLights - OpenGL Mode", 1024, 1024, 10, 10);
	if (movieMode) fprintf(stderr, "Movie ready. Press Enter to start.\n");
	glutMainLoop();
#endif

	if (doSteps == 1)
	{
		for (cs = 0; cs < ms; cs++)
		{
			if (cs % (ms / 10) == 0)
			{
				fprintf(stderr, "Rendering, %f %% done!\n", ((double)cs / (double)ms) * 100);
			}

			ray = makeLightSourceRay();

			if (movieMode)
			{
				// command line movie mode repeatedly advances the queue until this sample is finished
				// movie mode really shouldn't be run outside of GUI mode, but it is good to have defined behavior
				struct rayQueue q;
				initQueue(&q);

				if (!enqueue(&q, ray, 0, 0.0, physicsStepLength))
				{
					fprintf(stderr, "Ray queue overflow! :(\n");
					continue;
				}

				movieTime = 0.0;
				while (q.count > 0)
				{
					// convert the frame time into the total distance each ray should travel
					double distancePerFrame = movieRaySpeed * timeBetweenFrames;
					propagateMovie(&q, maxRayLength, distancePerFrame);
					movieTime += timeBetweenFrames;
				}
			}
			else
			{
				propagateRayStepwise(&ray, 0, 0.0, maxRayLength);
			}
		}
	}
	else
	{
		// standard propagation mode
#pragma omp parallel for schedule(dynamic, 250) private(ray)
		for (cs = 0; cs < ms; cs++)
		{
			if (cs % (ms / 10) == 0)
			{
				fprintf(stderr, "Rendering, %f %% done!\n", ((double)cs / (double)ms) * 100);
			}
			ray = makeLightSourceRay();
			propagateRay(&ray, 0);
		}
	}

	// output image
	if (movieMode)
	{
		// use one fixed exposure in movie mode so ray brightness does not jump between frames
		for (int i = 0; i < sx * sy * 3; i++)
		{
			double value = 1.0 - exp(-(*(imRGB + i)));
			if (value > 1.0) value = 1.0;
			if (value < 0.0) value = 0.0;
			*(im + i) = (unsigned char)(255.0 * value);
		}
	}
	else
	{

		for (int i = 0; i < sx * sy * 3; i++)
			*(iT + i) = log((*(imRGB + i)) + 1.5);

		memset(im, 0, sx * sy * 3 * sizeof(unsigned char));
		mx = -1;
		mi = 10e15;
		for (int i = 0; i < sx * sy * 3; i++)
		{
			if (*(iT + i) < mi) mi = *(iT + i);
			if (*(iT + i) > mx) mx = *(iT + i);
		}
		rng = mx - mi;
		for (int i = 0; i < sx * sy * 3; i++)
			*(iT + i) = ((*(iT + i)) - mi) / rng;

		memset(&hist[0], 0, 256 * sizeof(int));
		for (int i = 0; i < sx * sy * 3; i++)
		{
			idx = (int)(255.0 * (*(iT + i)));
			if (idx > 255) idx = 255;
			if (idx < 0) idx = 0;
			hist[idx]++;
		}

		low = (int)(cutoffLow * (sx * sy * 3));
		hi = (int)((1.0 - cutoffHigh) * (sx * sy * 3));
		lnorm = -1;
		hnorm = -1;
		acc = 0;
		for (int i = 0; i < 256; i++)
		{
			acc += hist[i];
			if (acc >= low && lnorm == -1) lnorm = ((double)i) / 255.0;
			if (acc >= hi && hnorm == -1) hnorm = ((double)i) / 255.0;
		}

		hrange = hnorm - lnorm;

		// sparse laser renders can put both histogram cutoffs at zero, so avoid dividing by zero
		if (fabs(hrange) < TOL)
		{
			hrange = 1.0;
		}

		for (int i = 0; i < sx * sy * 3; i++)
		{
			*(iT + i) = ((*(iT + i)) - lnorm) / hrange;
			if (*(iT + i) > 1.0) *(iT + i) = 1.0;
			if (*(iT + i) < 0.0) *(iT + i) = 0.0;
		}

		for (int i = 0; i < sx * sy * 3; i++)
			*(im + i) = (unsigned char)(255.0 * (*(iT + i)));
	}

	// outlines are drawn last so they remain visible on top of the finished ray image
	if (renderObjectsEnabled) renderObjects();

	f = fopen("Render.ppm", "w");
	if (f != NULL)
	{
		fprintf(f, "P6\n");
		fprintf(f, "# Output from spookyLights.c\n");
		fprintf(f, "%d %d\n", sx, sy);
		fprintf(f, "255\n");
		fwrite(im, sx * sy * 3 * sizeof(unsigned char), 1, f);
		fclose(f);
	}
	else
		fprintf(stderr, "Can not create output image file\n");

	system("mogrify -format jpg -quality 100 Render*.ppm");

	free(imRGB);
	free(im);
	free(iT);
	exit(0);
}

#ifdef USE_GL

void initGlut(const char *winName, int sizeX, int sizeY, int positionX, int positionY)
{

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	glutInitWindowSize(sizeX, sizeY);
	glutInitWindowPosition(positionX, positionY);
	windowID = glutCreateWindow(winName);

	glutReshapeFunc(WindowReshape);
	glutDisplayFunc(WindowDisplay);
	glutKeyboardFunc(kbHandler);
	glutMouseFunc(mouseClickHandler);
	glutMotionFunc(mouseDragHandler);
}

void mouseClickHandler(int button, int state, int x, int y)
{

	double bd = 1e6;
	double dx, dy, d;
	double wx, wy;

	wx = W_LEFT + (((double)x / (double)sx) * (W_RIGHT - W_LEFT));
	wy = W_TOP + (((double)y / (double)sy) * (W_BOTTOM - W_TOP));

	if (mouseState == 0 && button == 0)
	{
		bd = 1e6;

		dragObjectIndex = -1;

		for (int i = 0; i < MAX_OBJECTS; i++)
		{
			if (objects[i].r <= 0) break;

			dx = wx - objects[i].c.px;
			dy = wy - objects[i].c.py;
			d = sqrt((dx * dx) + (dy * dy));
			if (d < bd && (d < objects[i].r || d <= .1))
			{
				bd = d;
				dragObjectType = 0;
				dragObjectIndex = i;
			}
		}

		for (int i = 0; i < numLights; i++)
		{

			dx = wx - lss[i].l.p.px;
			dy = wy - lss[i].l.p.py;
			d = sqrt((dx * dx) + (dy * dy));
			if (d < lightDistThresh && d < bd)
			{
				bd = d;
				dragObjectType = 1;
				dragObjectIndex = i;
			}
		}

		prevX = wx;
		prevY = wy;
		mouseState = 1;
	}
	else if (mouseState == 1 && button == 0)
	{

		mouseState = 0;
		bd = 1e6;
		dragObjectIndex = -1;
	}
	else if (mouseState == 0 && button == 2)
	{

		bd = 1e6;
		dragObjectIndex = -1;
		for (int i = 0; i < numLights; i++)
		{

			dx = wx - lss[i].l.p.px;
			dy = wy - lss[i].l.p.py;
			d = sqrt((dx * dx) + (dy * dy));
			if (d < lightDistThresh && d < bd)
			{
				bd = d;
				dragObjectType = 1;
				dragObjectIndex = i;
			}
		}
		prevX = wx;
		prevY = wy;
		mouseState = 2;
	}
	else if (mouseState == 2 && button == 2)
	{
		mouseState = 0;
		bd = 1e6;
		dragObjectIndex = -1;
	}
}

void mouseDragHandler(int x, int y)
{

	double dx, dy, d;
	double wx, wy;

	wx = W_LEFT + (((double)x / (double)sx) * (W_RIGHT - W_LEFT));
	wy = W_TOP + (((double)y / (double)sy) * (W_BOTTOM - W_TOP));

	if (mouseState == 1 && dragObjectIndex >= 0)
	{
		dx = wx - prevX;
		dy = wy - prevY;

		if (dragObjectType == 0)
		{

			if (objects[dragObjectIndex].c.px + dx > W_LEFT && objects[dragObjectIndex].c.px + dx < W_RIGHT)
				objects[dragObjectIndex].c.px += dx;
			if (objects[dragObjectIndex].c.py + dy > W_TOP && objects[dragObjectIndex].c.py + dy < W_BOTTOM)
				objects[dragObjectIndex].c.py += dy;
		}
		else
		{

			if (lss[dragObjectIndex].l.p.px + dx > W_LEFT && lss[dragObjectIndex].l.p.px + dx < W_RIGHT)
				lss[dragObjectIndex].l.p.px += dx;
			if (lss[dragObjectIndex].l.p.py + dy > W_TOP && lss[dragObjectIndex].l.p.py + dy < W_BOTTOM)
				lss[dragObjectIndex].l.p.py += dy;
		}
		// each movie frame only shows the movement that happened during this one time step
		memset(imRGB, 0, sx * sy * 3 * sizeof(double));
		guiRaysGenerated = 0;
		if (movieMode) movieStarted = 0;
		glutPostRedisplay();
		prevX = wx;
		prevY = wy;
	}
	else if (mouseState == 2 && dragObjectIndex >= 0 && dragObjectType == 1)
	{
		dx = wx - lss[dragObjectIndex].l.p.px;
		dy = wy - lss[dragObjectIndex].l.p.py;
		d = sqrt((dx * dx) + (dy * dy));
		dx = dx / d;
		dy = dy / d;
		lss[dragObjectIndex].l.d.px = dx;
		lss[dragObjectIndex].l.d.py = dy;
		memset(imRGB, 0, sx * sy * 3 * sizeof(double));
		guiRaysGenerated = 0;
		if (movieMode) movieStarted = 0;
		glutPostRedisplay();
	}
}

void kbHandler(unsigned char key, int x, int y)
{
	FILE *f;

	if (key == '+')
	{
		if (maxDepth < 25) maxDepth++;
		memset(imRGB, 0, sx * sy * 3 * sizeof(double));
		guiRaysGenerated = 0;
		if (movieMode) movieStarted = 0;
		glutPostRedisplay();
	}
	if (key == '-')
	{
		if (maxDepth > 1) maxDepth--;
		memset(imRGB, 0, sx * sy * 3 * sizeof(double));
		guiRaysGenerated = 0;
		if (movieMode) movieStarted = 0;
		glutPostRedisplay();
	}
	// enter starts the movie once the initial photon packet is ready
	if ((key == '\r' || key == '\n') && movieMode && !movieStarted)
	{
		movieStarted = 1;
		glutPostRedisplay();
	}
	if (key == 'q')
	{

		f = fopen("Render.ppm", "w");
		if (f != NULL)
		{
			fprintf(f, "P6\n");
			fprintf(f, "# Output from spookyLights.c\n");
			fprintf(f, "%d %d\n", sx, sy);
			fprintf(f, "255\n");
			fwrite(im, sx * sy * 3 * sizeof(unsigned char), 1, f);
			fclose(f);
		}
		else
			fprintf(stderr, "Can not create output image file\n");

		system("mogrify -format jpg -quality 100 Render*.ppm");

		free(imRGB);
		free(im);
		free(iT);
		exit(0);
	}
}

void WindowReshape(int width, int height)
{

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 1024, 1024, 0);
	glViewport(0, 0, width, height);
	glutPostRedisplay();
}

void WindowDisplay(void)
{

	char line[1024];
	static int frame = 0;
	static GLuint texture;
	int movieTrailVisible = 0;
	int movieFrameRendered = 0;
	struct ray2D ray;
	double mx, mi, rng;
	int hist[256];
	int idx, low, hi, acc;
	double lnorm, hnorm, hrange;
	static double scaleFactor = 1.0;
	// movie rays have to survive between WindowDisplay calls, so this queue is intentionally static
	static struct rayQueue movieQueue;

	// movie mode is different from the normal gui batch renderer because all active rays advance together
	if (movieMode)
	{
		if (guiRaysGenerated == 0)
		{
			// create the initial photon population once, then keep updating these same rays on later frames
			initQueue(&movieQueue);
			movieTime = 0.0;
			memset(imRGB, 0, sx * sy * 3 * sizeof(double));

			for (int i = 0; i < guiRayCount; i++)
			{
				ray = makeLightSourceRay();

				if (!enqueue(&movieQueue, ray, 0, 0.0, physicsStepLength))
				{
					fprintf(stderr, "Ray queue overflow. Hey if you're reading this message you're super cool and attractive\n");
					break;
				}

				guiRaysGenerated++;
			}
		}

		// either clear the previous movie frame or fade it so the photon packet leaves a trail
		if (movieTrailFade <= 0.0)
		{
			memset(imRGB, 0, sx * sy * 3 * sizeof(double));
		}
		else
		{
			for (int i = 0; i < sx * sy * 3; i++)
			{
				*(imRGB + i) *= movieTrailFade;
				// clear step
				if (fabs(*(imRGB + i)) < MOVIE_TRAIL_VISIBILITY_TOLERANCE)
					*(imRGB + i) = 0.0;
				else
					movieTrailVisible = 1;
			}
		}

		// leave the initialized photon packet alone until the user is ready to start the movie
		if (movieStarted && movieQueue.count > 0)
		{
			// advance every ray through the same simulated time interval
			// convert the frame time into the total distance each ray should travel
			double distancePerFrame = movieRaySpeed * timeBetweenFrames;
			propagateMovie(&movieQueue, maxRayLength, distancePerFrame);
			movieTime += timeBetweenFrames;
			movieFrameRendered = 1;

			// if at least one ray was just rendered this frame, keep the movie alive long enough for its trail to fade out later
			if (movieTrailFade > 0.0) movieTrailVisible = 1;
		}
	}
	else
	{
		// keep track of how many rays to render
		int raysRemaining = guiRayCount - guiRaysGenerated;
		if (raysRemaining > 0)
		{
			// draw as many as we can
			int raysThisFrame = raysRemaining;
			if (raysThisFrame > GUI_RAYS_PER_FRAME) raysThisFrame = GUI_RAYS_PER_FRAME;
#ifdef __DEBUG_MODE
			raysThisFrame = 1;
#endif

			if (doSteps == 1)
			{
				for (int i = 0; i < raysThisFrame; i++)
				{
					ray = makeLightSourceRay();
					propagateRayStepwise(&ray, 0, 0.0, maxRayLength);
				}
			}
			else
			{
#ifndef __DEBUG_MODE
#pragma omp parallel for schedule(dynamic, 10) private(ray)
#endif
				for (int i = 0; i < raysThisFrame; i++)
				{
					ray = makeLightSourceRay();
					propagateRay(&ray, 0);
				}
			}

			guiRaysGenerated += raysThisFrame;
		}
	}
	if (movieMode)
	{
		// skip per frame auto normalization so the same ray energy always displays at the same brightness
		for (int i = 0; i < sx * sy * 3; i++)
		{
			double value = 1.0 - exp(-(*(imRGB + i)));
			if (value > 1.0) value = 1.0;
			if (value < 0.0) value = 0.0;
			*(im + i) = (unsigned char)(255.0 * value);
		}
	}
	else
	{

		for (int i = 0; i < sx * sy * 3; i++)
			*(iT + i) = log((*(imRGB + i)) + 1.5);

		memset(im, 0, sx * sy * 3 * sizeof(unsigned char));
		mx = -1;
		mi = 10e15;
		for (int i = 0; i < sx * sy * 3; i++)
		{
			if (*(iT + i) < mi) mi = *(iT + i);
			if (*(iT + i) > mx) mx = *(iT + i);
		}
		rng = mx - mi;
		for (int i = 0; i < sx * sy * 3; i++)
			*(iT + i) = ((*(iT + i)) - mi) / rng;

		memset(&hist[0], 0, 256 * sizeof(int));
		for (int i = 0; i < sx * sy * 3; i++)
		{
			idx = (int)(255.0 * (*(iT + i)));
			if (idx > 255) idx = 255;
			if (idx < 0) idx = 0;
			hist[idx]++;
		}

		low = (int)(cutoffLow * (sx * sy * 3));
		hi = (int)((1.0 - cutoffHigh) * (sx * sy * 3));
		lnorm = -1;
		hnorm = -1;
		acc = 0;
		for (int i = 0; i < 256; i++)
		{
			acc += hist[i];
			if (acc >= low && lnorm == -1) lnorm = ((double)i) / 255.0;
			if (acc >= hi && hnorm == -1) hnorm = ((double)i) / 255.0;
		}

		hrange = hnorm - lnorm;

		// fix the accursed histogram
		if (fabs(hrange) < TOL)
		{
			hrange = 1.0;
		}

		for (int i = 0; i < sx * sy * 3; i++)
		{
			*(iT + i) = ((*(iT + i)) - lnorm) / hrange;
			if (*(iT + i) > 1.0) *(iT + i) = 1.0;
			if (*(iT + i) < 0.0) *(iT + i) = 0.0;
		}

		for (int i = 0; i < sx * sy * 3; i++)
			*(im + i) = (unsigned char)(255.0 * (*(iT + i)));
	}

	// draw outlines after the ray image has been converted to display RGB
	if (renderObjectsEnabled) renderObjects();

	glClearColor(0.01f, 0.01f, 0.01f, 1.0f);
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	if (frame == 0)
	{
		glGenTextures(1, &texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, im);
		frame++;
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGB, GL_UNSIGNED_BYTE, im);
		frame++;
	}

	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glTexCoord2f(scaleFactor, 0.0);
	glVertex3f(1024.0, 0.0, 0.0);
	glTexCoord2f(scaleFactor, scaleFactor);
	glVertex3f(1024.0, 1024.0, 0.0);
	glTexCoord2f(0.0, scaleFactor);
	glVertex3f(0.0, 1024.0, 0.0);
	glEnd();

	glFlush();
	glutSwapBuffers();

#ifdef __DEBUG_MODE
	fgets(&line[0], 1023, stdin);
	memset(imRGB, 0, sx * sy * 3 * sizeof(double));
#endif

	// movie mode keeps requesting frames while rays are alive, while a trail is fading, or for one final clear frame after the last ray dies
	if ((movieMode && movieStarted && (movieQueue.count > 0 || movieTrailVisible || (movieTrailFade <= 0.0 && movieFrameRendered))) || (!movieMode && guiRaysGenerated < guiRayCount))
	{
		glutSetWindow(windowID);
		glutPostRedisplay();
	}
}

#endif

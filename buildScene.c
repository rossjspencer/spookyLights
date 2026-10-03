#define W_TOP -2.00
#define W_BOTTOM 2.00

#define W_LEFT -2.00
#define W_RIGHT 2.00

#include "spookyLights.h"

void buildWalls(void)
{
	struct point2D p, d;
	p.px = W_LEFT;
	p.py = W_TOP;
	d.px = W_RIGHT - W_LEFT;
	d.py = 0;
	walls[0].w.p = p;
	walls[0].w.d = d;
	walls[0].materialType = 1;
	walls[0].red = 1.0;
	walls[0].green = 1.0;
	walls[0].blue = 1.0;

	p.px = W_RIGHT;
	p.py = W_TOP;
	d.px = 0;
	d.py = W_BOTTOM - W_TOP;
	walls[1].w.p = p;
	walls[1].w.d = d;
	walls[1].materialType = 1;
	walls[1].red = 1.0;
	walls[1].green = 1.0;
	walls[1].blue = 1.0;

	p.px = W_RIGHT;
	p.py = W_BOTTOM;
	d.px = W_LEFT - W_RIGHT;
	d.py = 0;
	walls[2].w.p = p;
	walls[2].w.d = d;
	walls[2].materialType = 1;
	walls[2].red = 1.0;
	walls[2].green = 1.0;
	walls[2].blue = 1.0;

	p.px = W_LEFT;
	p.py = W_BOTTOM;
	d.px = 0;
	d.py = W_TOP - W_BOTTOM;
	walls[3].w.p = p;
	walls[3].w.d = d;
	walls[3].materialType = 1;
	walls[3].red = 1.0;
	walls[3].green = 1.0;
	walls[3].blue = 1.0;
}

void parseScene(char *name)
{

	FILE *f;
	char line[1024];
	char *cp;
	double x, y, dX, dY, r, idx, red, green, blue, mass;
	int type;
	struct point2D c, d;
	struct ray2D l;

	f = fopen(name, "r");
	if (f == NULL)
	{
		fprintf(stderr, "Unable to open scene description file\n");
		exit(0);
	}

	while (fgets(&line[0], 1024, f))
	{
		if (line[0] != '#')
		{
			cp = strstr(line, "circle");
			if (cp != NULL)
			{
				sscanf(cp + 6, "%lf %lf %lf %lf %lf %lf %lf %d", &x, &y, &r, &idx, &red, &green, &blue, &type);
				fprintf(stderr, "Adding a circle, with parameters (%lf, %lf), r=%lf, idx=%lf, [%lf, %lf, %lf], %d\n", x, y, r, idx, red, green, blue, type);
				c.px = x;
				c.py = y;
				addCirc(&c, r, type, idx, red, green, blue);
			}
			cp = strstr(line, "light");
			if (cp != NULL)
			{
				sscanf(cp + 5, "%lf %lf %lf %lf %lf %lf %lf %d", &x, &y, &dX, &dY, &red, &green, &blue, &type);
				fprintf(stderr, "Adding a light with parameters c=(%lf, %lf), d=(%lf, %lf), [%lf, %lf, %lf], %d\n", x, y, dX, dY, red, green, blue, type);
				c.px = x;
				c.py = y;
				d.px = dX;
				d.py = dY;
				l.p = c;
				l.d = d;
				lss[numLights].l = l;
				lss[numLights].lightType = type;
				lss[numLights].red = red;
				lss[numLights].green = green;
				lss[numLights].blue = blue;
				numLights++;
			}
			// black holes use their own scene entry since they need a position and mass instead of a radius/material
			cp = strstr(line, "black_hole");
			if (cp != NULL)
			{
				sscanf(cp + 10, "%lf %lf %lf", &x, &y, &mass);
				fprintf(stderr, "Adding a BLACK HOLE!!! with parameters c=(%lf, %lf), mass=%lf\n", x, y, mass);
				c.px = x;
				c.py = y;
				addBlackHole(&c, mass);
			}
		}
	}
	fclose(f);
}

void buildScene(void)
{
	parseScene(sceneName);
}

#ifndef RAYS_2D_H

#define RAYS_2D_H

#include "spookyLights.h"

#define W_TOP -2.00
#define W_BOTTOM 2.00
#define W_LEFT -2.00
#define W_RIGHT 2.00

struct ray2D makeLightSourceRay(void);

void propagateRay(struct ray2D *ray, int depth);

// advances one ray with the stepwise propagation rules
void propagateRayStepwise(struct ray2D *ray, int depth, double distanceTravelled, double maxLength);

// applies black hole gravity over one integration step
int accelerateRay(struct ray2D *ray, double integrationDistance);

// helpers for the circular queue used by movie propagation
void initQueue(struct rayQueue *q);
int enqueue(struct rayQueue *q, struct ray2D ray, int depth, double distanceTravelled, double physicsDistanceRemaining);
int dequeue(struct rayQueue *q, struct ray2D *ray, int *depth, double *distanceTravelled, double *physicsDistanceRemaining);

// advances every active movie ray through one frame of simulated time
void propagateMovie(struct rayQueue *queue, double maxLength, double distancePerFrame);

void intersectRay(struct ray2D *ray, struct point2D *p, struct point2D *n, double *lambda, int *type, double *refractiveIndex, double *red, double *green, double *blue);

// same idea as intersectRay, but only checks the four walls
int findWallCollision(struct ray2D *ray, struct point2D *p, struct point2D *n, double *lambda, int *type, double *red, double *green, double *blue);

void initializeSourceSpectra(void);

#endif

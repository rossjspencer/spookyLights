#include "rays2D.h"

#include <float.h>

#define SOURCE_SPECTRUM_SAMPLES 256

static double hueToWavelength(double hue);
static double sourceSpectralCDF[MAX_LIGHTS][SOURCE_SPECTRUM_SAMPLES];
static double sourceSpectralTotals[MAX_LIGHTS];
static double sourceScales[MAX_LIGHTS];

double getSpectralRefractiveIndex(double baseIndex, double hue)
{
	// tune this to make the dispersion actually visible at reasonable dispersion values
	double dispersionScale = 50.0;

	// convert the ray's hue into an approximate visible wavelength in nanometers
	double wavelength = hueToWavelength(hue);

	// cauchy's equation uses wavelength more conveniently in micrometers
	double lambda = wavelength / 1000.0;

	// cauchy's B coefficient controls how strongly refractive index changes with wavelength
	double coefficientB = 0.004;

	// use green light as the wavelength where baseIndex should be exactly correct, chosen because it's sort of in the
	// middle of the spectrum. this unfortunately needs to be done in order to keep the original meaning of
	// refractiveIndex in the scene. it is decently accurate regardless
	double referenceLambda = 0.55;

	// solve cauchy's equation backwards to get A from the given base refractive index
	double coefficientA = baseIndex - coefficientB / (referenceLambda * referenceLambda);

	// calculate the refractive index for this wavelength using cauchy's equation
	double cauchyIndex = coefficientA + coefficientB / (lambda * lambda);

	// scale only the wavelength-dependent change so dispersion = 0 keeps the old behavior
	return baseIndex + dispersionScale * dispersion * (cauchyIndex - baseIndex);
}

static int rayBelowMinIntensity(struct ray2D *ray)
{
	// only stop a ray once every colour channel has fallen below the requested cutoff
	if (minRayIntensity <= 0.0) return 0;

	double strongestChannel = ray->red;
	if (ray->green > strongestChannel) strongestChannel = ray->green;
	if (ray->blue > strongestChannel) strongestChannel = ray->blue;

	return strongestChannel < minRayIntensity;
}

static void applyMaterialColour(struct ray2D *ray, double red, double green, double blue)
{
	// multiply the ray energy in each colour channel by the material colour
	ray->red *= red;
	ray->green *= green;
	ray->blue *= blue;
}

static void getReflectedDirection(struct ray2D *ray, struct point2D *normal, struct point2D *result)
{
	// starndard reflection formula
	double projection = 2 * dot(&ray->d, normal);
	result->px = ray->d.px - projection * normal->px;
	result->py = ray->d.py - projection * normal->py;
	normalize(result);
}

static double getFresnelReflectance(struct ray2D *ray, struct point2D *normal, double refractiveIndex)
{
	// use the exact unpolarized fresnel equations for an air/material boundary
	double n1;
	double n2;

	if (ray->insideOut)
	{
		n1 = refractiveIndex;
		n2 = 1.0;
	}
	else
	{
		n1 = 1.0;
		n2 = refractiveIndex;
	}

	struct point2D adjNormal = *normal;
	double directionDot = dot(&ray->d, &adjNormal);
	if (directionDot > 0)
	{
		adjNormal.px = -adjNormal.px;
		adjNormal.py = -adjNormal.py;
		directionDot = -directionDot;
	}

	double cosIncident = -directionDot;
	if (cosIncident < 0.0) cosIncident = 0.0;
	if (cosIncident > 1.0) cosIncident = 1.0;

	double refractiveRatio = n1 / n2;
	double sinTransmittedSquared = refractiveRatio * refractiveRatio * (1.0 - cosIncident * cosIncident);

	// total internal reflection means all of the light is reflected
	if (sinTransmittedSquared >= 1.0)
	{
		return 1.0;
	}

	double cosTransmitted = sqrt(1.0 - sinTransmittedSquared);

	// calculate reflectances for both parallel and perpendicular polarization
	double rsNumerator = n1 * cosIncident - n2 * cosTransmitted;
	double rsDenominator = n1 * cosIncident + n2 * cosTransmitted;
	double rpNumerator = n1 * cosTransmitted - n2 * cosIncident;
	double rpDenominator = n1 * cosTransmitted + n2 * cosIncident;

	double rs = 0.0;
	double rp = 0.0;

	if (fabs(rsDenominator) > TOL)
	{
		rs = rsNumerator / rsDenominator;
		rs *= rs;
	}

	if (fabs(rpDenominator) > TOL)
	{
		rp = rpNumerator / rpDenominator;
		rp *= rp;
	}

	// assume the light is unpolarized to simplify the equation
	double reflectance = 0.5 * (rs + rp);
	if (reflectance < 0.0) reflectance = 0.0;
	if (reflectance > 1.0) reflectance = 1.0;
	return reflectance;
}

// defines a gaussian curve centered around a given wavelength with a given width
static double spectralGaussian(double wavelength, double center, double width)
{
	double d = (wavelength - center) / width;
	return exp(-0.5 * d * d);
}

static double hueToWavelength(double hue)
{
	return 700.0 - 300.0 * hue;
}

static double sourceSpectralPower(struct light2D *light, double hue)
{
	// model the source spd as rgb-weighted spectral power bell curves across the visible range
	double wavelength = hueToWavelength(hue);
	double redPower = light->red * spectralGaussian(wavelength, 620.0, 45.0);
	double greenPower = light->green * spectralGaussian(wavelength, 535.0, 35.0);
	double bluePower = light->blue * spectralGaussian(wavelength, 460.0, 30.0);
	return redPower + greenPower + bluePower;
}

void initializeSourceSpectra(void)
{
	if (dispersion <= 0) return;

	for (int lightIndex = 0; lightIndex < numLights; lightIndex++)
	{
		double total = 0.0;

		// find the total power over all samples
		for (int i = 0; i < SOURCE_SPECTRUM_SAMPLES; i++)
		{
			double hue = ((double)i + 0.5) / (double)SOURCE_SPECTRUM_SAMPLES;
			total += sourceSpectralPower(&lss[lightIndex], hue);
			sourceSpectralCDF[lightIndex][i] = total;
		}

		sourceSpectralTotals[lightIndex] = total;
		sourceScales[lightIndex] = fmax(lss[lightIndex].red, fmax(lss[lightIndex].green, lss[lightIndex].blue));
	}
}

static double sampleSourceHue(int lightIndex)
{
	// sample the modeled spd
	double total = sourceSpectralTotals[lightIndex];

	if (total <= 0.0)
	{
		return (double)rand() / (double)RAND_MAX;
	}

	// pick a random value in the possible range
	double target = ((double)rand() / (double)RAND_MAX) * total;
	int low = 0;
	int high = SOURCE_SPECTRUM_SAMPLES - 1;

	// find where the target is on the spectrum with binary search
	while (low < high)
	{
		int middle = low + (high - low) / 2;
		if (sourceSpectralCDF[lightIndex][middle] >= target)
		{
			high = middle;
		}
		else
		{
			low = middle + 1;
		}
	}

	// add small variance to the wavelength so we aren't capped at 256 colors
	double variance = (double)rand() / (double)RAND_MAX;
	return ((double)low + variance) / (double)SOURCE_SPECTRUM_SAMPLES;
}

static int getRefractedDirection(struct ray2D *ray, struct point2D *normal, double refractiveIndex, struct point2D *result)
{
	double refractionComponent;

	if (ray->insideOut == 0)
	{
		refractionComponent = 1.0 / refractiveIndex;
	}
	else
	{
		refractionComponent = refractiveIndex;
	}

	// make sure the normal faces the incoming ray
	struct point2D adjNormal = *normal;
	double directionDot = dot(&ray->d, &adjNormal);
	if (directionDot > 0)
	{
		adjNormal.px = -adjNormal.px;
		adjNormal.py = -adjNormal.py;
		directionDot = -directionDot;
	}

	double cosIncident = -directionDot;
	double cosSquared = 1.0 - refractionComponent * refractionComponent * (1.0 - cosIncident * cosIncident);

	// return false when total internal reflection occurs
	if (cosSquared < 0)
	{
		return 0;
	}

	result->px = refractionComponent * ray->d.px + (refractionComponent * cosIncident - sqrt(cosSquared)) * adjNormal.px;
	result->py = refractionComponent * ray->d.py + (refractionComponent * cosIncident - sqrt(cosSquared)) * adjNormal.py;
	normalize(result);
	return 1;
}

struct ray2D makeLightSourceRay(void)
{
	struct ray2D ray;

	ray.p.px = 0;
	ray.p.py = 0;
	ray.d.px = 1;
	ray.d.py = 0;
	ray.red = 0;
	ray.green = 0;
	ray.blue = 0;
	ray.insideOut = 0;
	ray.monochromatic = 0;
	ray.hue = 0;

	if (numLights <= 0)
	{
		fprintf(stderr, "No light sources in scene :(\n");
		exit(1);
	}

	// get a random light index
	int lightIndex = rand() % numLights;
	struct light2D *chosenLight = &lss[lightIndex];

	// handle position
	ray.p.px = chosenLight->l.p.px;
	ray.p.py = chosenLight->l.p.py;

	// sample a monochromatic source ray from the source spd when dispersion is enabled
	if (dispersion > 0)
	{
		double sourceScale = sourceScales[lightIndex];
		ray.hue = sampleSourceHue(lightIndex);
		hue2RGB(ray.hue, &ray.red, &ray.green, &ray.blue);
		ray.red *= sourceScale;
		ray.green *= sourceScale;
		ray.blue *= sourceScale;
		ray.monochromatic = 1;
	}
	else
	{
		ray.red = chosenLight->red;
		ray.green = chosenLight->green;
		ray.blue = chosenLight->blue;
		ray.monochromatic = 0;
		ray.hue = 0;
	}

	// point light source
	if (chosenLight->lightType == 0)
	{
		// generate random double between 0 and 1, then scale to an angle
		double lambda = (double)rand() / RAND_MAX * 2.0 * PI;

		// use to get a point on the unit circle, so the direction vector is always unit length
		ray.d.px = cos(lambda);
		ray.d.py = sin(lambda);
	}
	// laser light source
	else if (chosenLight->lightType == 1)
	{
		ray.d.px = chosenLight->l.d.px;
		ray.d.py = chosenLight->l.d.py;
		normalize(&ray.d);
	}
	else
	{
		fprintf(stderr, "What the hell kind of light type is %d?\n", chosenLight->lightType);
	}

	return (ray);
}

void propagateRay(struct ray2D *ray, int depth)
{
	// if max recursion depth has been reached, stop
	if (depth >= maxDepth) return;
	if (rayBelowMinIntensity(ray)) return;

	double lambda = DBL_MAX; // initializing lambda to be very large ensures intersectRay works as intended
	struct point2D intersectionPoint;
	struct point2D normal;
	int collisionType;
	double refractiveIndex;
	double red;
	double green;
	double blue;

	intersectRay(ray, &intersectionPoint, &normal, &lambda, &collisionType, &refractiveIndex, &red, &green, &blue);

	int hitWall = findWallCollision(ray, &intersectionPoint, &normal, &lambda, &collisionType, &red, &green, &blue);

	if (lambda == DBL_MAX)
	{
		return;
	}

	renderRay(&ray->p, &intersectionPoint, ray->red, ray->green, ray->blue);

	if (hitWall && !doWallCollision)
	{
		return;
	}

	if (collisionType == 0) // mirror
	{
		// use the standard reflection formula r = v - 2(v * n)n
		struct point2D mirrorResult;
		getReflectedDirection(ray, &normal, &mirrorResult);

		struct ray2D mirrorRay = *ray;
		mirrorRay.p = intersectionPoint;
		mirrorRay.d = mirrorResult;

		// the material colour absorbs the corresponding parts of the incoming ray
		applyMaterialColour(&mirrorRay, red, green, blue);

		propagateRay(&mirrorRay, depth + 1);
	}
	else if (collisionType == 1) // scatter
	{
		struct point2D scatterResult;

		// generates a random angle between -pi/2 and pi/2
		double angle = ((double)rand() / RAND_MAX * PI) - PI / 2;

		// rotate the normal to get the result
		scatterResult.px = normal.px * cos(angle) - normal.py * sin(angle);
		scatterResult.py = normal.px * sin(angle) + normal.py * cos(angle);
		normalize(&scatterResult);

		struct ray2D scatterRay = *ray;
		scatterRay.p = intersectionPoint;
		scatterRay.d = scatterResult;

		// scattering surfaces filter the colour and absorb some of the total energy
		applyMaterialColour(&scatterRay, red, green, blue);
		scatterRay.red *= scatterRetention;
		scatterRay.green *= scatterRetention;
		scatterRay.blue *= scatterRetention;

		propagateRay(&scatterRay, depth + 1);
	}
	else // refract
	{
		// monochromatic rays keep their wavelength-dependent refractive index on later collisions
		double activeIndex = refractiveIndex;
		if (ray->monochromatic == 1 && dispersion > 0)
		{
			activeIndex = getSpectralRefractiveIndex(refractiveIndex, ray->hue);
		}

		// filter the ray by the material before deciding whether this sample reflects or refracts
		applyMaterialColour(ray, red, green, blue);

		// fresnel reflectance is also the probability that this sample reflects
		double reflectance = getFresnelReflectance(ray, &normal, activeIndex);
		double choice = (double)rand() / (double)RAND_MAX;

		if (choice < reflectance)
		{
			struct point2D reflectedDirection;
			getReflectedDirection(ray, &normal, &reflectedDirection);

			struct ray2D reflectedRay = *ray;
			reflectedRay.p = intersectionPoint;
			reflectedRay.d = reflectedDirection;

			propagateRay(&reflectedRay, depth + 1);
		}
		else
		{
			struct point2D refractedDirection;

			// total internal reflection should already have reflectance 1 but, you can't be too safe
			if (!getRefractedDirection(ray, &normal, activeIndex, &refractedDirection))
			{
				struct point2D reflectedDirection;
				getReflectedDirection(ray, &normal, &reflectedDirection);

				struct ray2D reflectedRay = *ray;
				reflectedRay.p = intersectionPoint;
				reflectedRay.d = reflectedDirection;
				propagateRay(&reflectedRay, depth + 1);
				return;
			}

			struct ray2D refractedRay = *ray;
			refractedRay.p = intersectionPoint;
			refractedRay.d = refractedDirection;
			refractedRay.insideOut = !ray->insideOut;

			propagateRay(&refractedRay, depth + 1);
		}
	}
	return;
}

void propagateRayStepwise(struct ray2D *ray, int depth, double distanceTravelled, double maxLength)
{
	// stop if the maximum interaction depth has been reached
	if (depth >= maxDepth) return;
	if (rayBelowMinIntensity(ray)) return;

	// follow this ray until it collides, reaches the length limit, or falls into a black hole
	while (distanceTravelled < maxLength && depth < maxDepth)
	{
		double currentStep = physicsStepLength;
		if (distanceTravelled + currentStep > maxLength)
		{
			currentStep = maxLength - distanceTravelled;
		}

		double lambda = DBL_MAX;
		struct point2D endpoint;
		struct point2D normal;
		int collisionType;
		double refractiveIndex;
		double red;
		double green;
		double blue;

		// stop if the ray has left the scene, this should only happen in rare edge cases that I can't even explain
		if (ray->p.px < W_LEFT - TOL || ray->p.px > W_RIGHT + TOL || ray->p.py < W_TOP - TOL || ray->p.py > W_BOTTOM + TOL)
		{
			return;
		}

		// check for the closest object and wall collision
		intersectRay(ray, &endpoint, &normal, &lambda, &collisionType, &refractiveIndex, &red, &green, &blue);

		int hitWall = findWallCollision(ray, &endpoint, &normal, &lambda, &collisionType, &red, &green, &blue);

		if (lambda <= currentStep && hitWall && !doWallCollision)
		{
			// draw up to the wall, then kill the ray
			renderRay(&ray->p, &endpoint, ray->red, ray->green, ray->blue);
			return;
		}

		// move one step if there is no collision within this step
		if (lambda > currentStep)
		{
			endpoint.px = ray->p.px + currentStep * ray->d.px;
			endpoint.py = ray->p.py + currentStep * ray->d.py;

			// again, stop if this step somehow leaves the scene without detecting a wall (why does this ever happen???)
			if (endpoint.px < W_LEFT - TOL || endpoint.px > W_RIGHT + TOL || endpoint.py < W_TOP - TOL || endpoint.py > W_BOTTOM + TOL)
			{
				return;
			}

			// draw this segment and move the ray to its endpoint
			renderRay(&ray->p, &endpoint, ray->red, ray->green, ray->blue);
			ray->p = endpoint;
			distanceTravelled += currentStep;

			// bend the direction for the next segment and stop if the ray entered a black hole
			if (accelerateRay(ray, currentStep)) return;

			continue;
		}

		// render only up to the collision point
		renderRay(&ray->p, &endpoint, ray->red, ray->green, ray->blue);
		ray->p = endpoint;
		distanceTravelled += lambda;
		depth++;

		if (depth >= maxDepth) return;

		if (collisionType == 0)
		{
			struct point2D mirrorResult;

			// calculate the reflected direction
			getReflectedDirection(ray, &normal, &mirrorResult);
			ray->d = mirrorResult;

			// the material colour absorbs the corresponding parts of the incoming ray
			applyMaterialColour(ray, red, green, blue);
		}
		else if (collisionType == 1)
		{
			struct point2D scatterResult;
			double angle = ((double)rand() / RAND_MAX * PI) - PI / 2;

			// rotate the normal by a random angle to get the scattered direction
			scatterResult.px = normal.px * cos(angle) - normal.py * sin(angle);
			scatterResult.py = normal.px * sin(angle) + normal.py * cos(angle);
			normalize(&scatterResult);
			ray->d = scatterResult;

			// scattering surfaces filter the colour and absorb some of the total energy
			applyMaterialColour(ray, red, green, blue);
			ray->red *= scatterRetention;
			ray->green *= scatterRetention;
			ray->blue *= scatterRetention;
		}
		else
		{
			// monochromatic rays keep their wavelength-dependent refractive index on later collisions
			double activeIndex = refractiveIndex;
			if (ray->monochromatic == 1 && dispersion > 0)
			{
				activeIndex = getSpectralRefractiveIndex(refractiveIndex, ray->hue);
			}

			// filter the ray by the material before deciding whether this sample reflects or refracts
			applyMaterialColour(ray, red, green, blue);

			// fresnel reflectance is also the probability that this sample reflects
			double reflectance = getFresnelReflectance(ray, &normal, activeIndex);
			double choice = (double)rand() / (double)RAND_MAX;

			if (choice < reflectance)
			{
				struct point2D reflectedDirection;
				getReflectedDirection(ray, &normal, &reflectedDirection);
				ray->d = reflectedDirection;
			}
			else
			{
				struct point2D refractedDirection;

				// total internal reflection should already have reflectance 1 but, you can't be too safe
				if (!getRefractedDirection(ray, &normal, activeIndex, &refractedDirection))
				{
					struct point2D reflectedDirection;
					getReflectedDirection(ray, &normal, &reflectedDirection);
					ray->d = reflectedDirection;
				}
				else
				{
					ray->d = refractedDirection;
					ray->insideOut = !ray->insideOut;
				}
			}
		}

		if (rayBelowMinIntensity(ray)) return;
	}
}

void propagateMovie(struct rayQueue *queue, double maxLength, double distancePerFrame)
{
	// only process the rays that existed at the start of this frame
	int raysThisFrame = queue->count;

	for (int i = 0; i < raysThisFrame; i++)
	{
		struct ray2D ray;
		int depth;
		double distanceTravelled;
		double physicsDistanceRemaining;

		// take the next active ray out of the queue
		if (!dequeue(queue, &ray, &depth, &distanceTravelled, &physicsDistanceRemaining))
		{
			break;
		}

		// this ray is already finished
		if (depth >= maxDepth || distanceTravelled >= maxLength || rayBelowMinIntensity(&ray))
		{
			continue;
		}

		int rayAlive = 1;
		int currentDepth = depth;
		double frameDistanceRemaining = distancePerFrame;

		// never let one movie frame carry the ray beyond its total path limit
		if (distanceTravelled + frameDistanceRemaining > maxLength)
		{
			frameDistanceRemaining = maxLength - distanceTravelled;
		}

		// keep the physics schedule independent from where movie frame boundaries happen
		while (frameDistanceRemaining > TOL && rayAlive)
		{
			double substepDistance = physicsDistanceRemaining;
			if (substepDistance > frameDistanceRemaining)
			{
				substepDistance = frameDistanceRemaining;
			}

			if (distanceTravelled + substepDistance > maxLength)
			{
				substepDistance = maxLength - distanceTravelled;
			}

			if (substepDistance <= TOL)
			{
				rayAlive = 0;
				break;
			}

			double substepDistanceRemaining = substepDistance;

			// use the rest of this physics step even if a collision happens early
			while (substepDistanceRemaining > TOL && rayAlive)
			{
				double lambda = DBL_MAX;
				struct point2D endpoint;
				struct point2D normal;
				int collisionType;
				double refractiveIndex;
				double red;
				double green;
				double blue;

				// kill this ray if it somehow starts outside the scene
				if (ray.p.px < W_LEFT - TOL || ray.p.px > W_RIGHT + TOL || ray.p.py < W_TOP - TOL || ray.p.py > W_BOTTOM + TOL)
				{
					rayAlive = 0;
					break;
				}

				// check for the closest object and wall collision
				intersectRay(&ray, &endpoint, &normal, &lambda, &collisionType, &refractiveIndex, &red, &green, &blue);

				int hitWall = findWallCollision(&ray, &endpoint, &normal, &lambda, &collisionType, &red, &green, &blue);

				// with wall collisions disabled, still record the path up to the wall before killing the ray
				if (lambda <= substepDistanceRemaining && hitWall && !doWallCollision)
				{
					renderRay(&ray.p, &endpoint, ray.red, ray.green, ray.blue);

					ray.p = endpoint;
					distanceTravelled += lambda;
					rayAlive = 0;
					break;
				}

				// no collision occurs before this physics step ends
				if (lambda > substepDistanceRemaining)
				{
					endpoint.px = ray.p.px + substepDistanceRemaining * ray.d.px;
					endpoint.py = ray.p.py + substepDistanceRemaining * ray.d.py;

					// kill this ray if it somehow leaves the scene without detecting a wall
					if (endpoint.px < W_LEFT - TOL || endpoint.px > W_RIGHT + TOL || endpoint.py < W_TOP - TOL || endpoint.py > W_BOTTOM + TOL)
					{
						rayAlive = 0;
						break;
					}

					renderRay(&ray.p, &endpoint, ray.red, ray.green, ray.blue);

					ray.p = endpoint;
					distanceTravelled += substepDistanceRemaining;
					break;
				}

				// record movement from the current position to the collision
				renderRay(&ray.p, &endpoint, ray.red, ray.green, ray.blue);

				ray.p = endpoint;
				substepDistanceRemaining -= lambda;
				distanceTravelled += lambda;

				// this collision counts as another interaction
				currentDepth++;

				if (currentDepth >= maxDepth)
				{
					rayAlive = 0;
					break;
				}

				if (collisionType == 0)
				{
					struct point2D mirrorResult;

					getReflectedDirection(&ray, &normal, &mirrorResult);

					ray.d = mirrorResult;

					// the material colour absorbs the corresponding parts of the incoming ray
					applyMaterialColour(&ray, red, green, blue);
				}
				else if (collisionType == 1)
				{
					struct point2D scatterResult;

					// get a random scatter angle between 90 and -90 degrees
					double angle = ((double)rand() / RAND_MAX * PI) - PI / 2;

					scatterResult.px = normal.px * cos(angle) - normal.py * sin(angle);
					scatterResult.py = normal.px * sin(angle) + normal.py * cos(angle);

					normalize(&scatterResult);

					ray.d = scatterResult;

					// scattering surfaces filter the colour and absorb some of the total energy
					applyMaterialColour(&ray, red, green, blue);

					ray.red *= scatterRetention;
					ray.green *= scatterRetention;
					ray.blue *= scatterRetention;
				}
				else
				{
					double activeIndex = refractiveIndex;

					// monochromatic rays use their wavelength-dependent refractive index
					if (ray.monochromatic == 1 && dispersion > 0)
					{
						activeIndex = getSpectralRefractiveIndex(refractiveIndex, ray.hue);
					}

					// filter the ray before the Fresnel decision
					applyMaterialColour(&ray, red, green, blue);

					// fresnel reflectance is also the probability that this sample reflects
					double reflectance = getFresnelReflectance(&ray, &normal, activeIndex);

					double choice = (double)rand() / (double)RAND_MAX;

					if (choice < reflectance)
					{
						struct point2D reflectedDirection;

						getReflectedDirection(&ray, &normal, &reflectedDirection);

						ray.d = reflectedDirection;
					}
					else
					{
						struct point2D refractedDirection;

						// this should normally only fail for total internal reflection
						if (!getRefractedDirection(&ray, &normal, activeIndex, &refractedDirection))
						{
							struct point2D reflectedDirection;

							getReflectedDirection(&ray, &normal, &reflectedDirection);

							ray.d = reflectedDirection;
						}
						else
						{
							ray.d = refractedDirection;
							ray.insideOut = !ray.insideOut;
						}
					}
				}

				if (rayBelowMinIntensity(&ray))
				{
					rayAlive = 0;
				}
			}

			if (!rayAlive)
			{
				break;
			}

			frameDistanceRemaining -= substepDistance;
			physicsDistanceRemaining -= substepDistance;

			if (distanceTravelled >= maxLength - TOL)
			{
				rayAlive = 0;
				break;
			}

			// only recalculate gravity once a full physics step has actually been completed
			if (physicsDistanceRemaining <= TOL)
			{
				if (accelerateRay(&ray, physicsStepLength))
				{
					rayAlive = 0;
					break;
				}

				physicsDistanceRemaining = physicsStepLength;
			}
		}

		if (rayAlive)
		{
			// back of the line, buster
			if (!enqueue(queue, ray, currentDepth, distanceTravelled, physicsDistanceRemaining))
			{
				fprintf(stderr, "Ray queue overflow!!!! What!!!! No dude!!!!\n");
			}
		}
	}
}

void intersectRay(struct ray2D *ray, struct point2D *p, struct point2D *n, double *lambda, int *type, double *refractiveIndex, double *tR, double *tG, double *tB)
{
	struct point2D rayStart = ray->p;
	struct point2D rayDirection = ray->d;
	double a = dot(&rayDirection, &rayDirection);

	// continue for every valid object
	int i = 0;
	while (i < MAX_OBJECTS && objects[i].r != -1)
	{
		struct point2D center = objects[i].c;
		double radius = objects[i].r;

		// the equation of this circle would be given by
		// (x - a)^2 + (y - b)^2 = r^2, where the circle is centered at (a, b)
		// for C = [x-a, y-b]T, we have ||C||^2 = r^2.
		// alternatively, for C = [x, y]T, p2 = [a, b], ||C - p2||^2 = r^2.

		// sub in ray equation: (p + lambda*d - p2)(p + lambda*d - p2) - r^2 = 0;
		// so, for z = p - p2, z * z + 2lambda(d * z) + lambda^2(d * d) - r^2 = 0;
		// this is a quadratic in lambda with a = d * d, b = 2(d * z), c = z * z - r^2.
		// find these dot products:
		struct point2D z;
		z.px = rayStart.px - center.px;
		z.py = rayStart.py - center.py;

		double b = 2 * dot(&rayDirection, &z);
		double c = dot(&z, &z) - radius * radius;

		double discriminant = b * b - 4 * a * c;

		// if no solution, skip
		if (discriminant < 0)
		{
			i++;
			continue;
		}

		// find both roots
		double root = sqrt(discriminant);
		double sol1 = (-b + root) / (2 * a);
		double sol2 = (-b - root) / (2 * a);

		double bestSol = -1;

		// check first solution
		if (sol1 > TOL && sol1 < *lambda)
		{
			bestSol = sol1;
		}

		// check second solution
		if (sol2 > TOL && sol2 < *lambda)
		{
			if (bestSol < 0 || sol2 < bestSol)
			{
				bestSol = sol2;
			}
		}

		// if a valid closer intersection was found update everything
		if (bestSol >= 0)
		{
			// update lambda
			*lambda = bestSol;

			// update collision point
			p->px = rayStart.px + rayDirection.px * *lambda;
			p->py = rayStart.py + rayDirection.py * *lambda;

			// update normal
			n->px = p->px - center.px;
			n->py = p->py - center.py;
			normalize(n);

			// update object type
			*type = objects[i].materialType;

			// update index of refraction
			*refractiveIndex = objects[i].refractiveIndex;

			// update material colour
			*tR = objects[i].red;
			*tG = objects[i].green;
			*tB = objects[i].blue;
		}

		i++;
	}
}

int findWallCollision(struct ray2D *ray, struct point2D *p, struct point2D *n, double *lambda, int *type, double *tR, double *tG, double *tB)
{
	int hitWall = 0;
	struct point2D rayStart = ray->p;
	struct point2D rayDirection = ray->d;

	for (int i = 0; i < 4; i++)
	{
		struct ray2D wallRay = walls[i].w;
		struct point2D wallStart = wallRay.p;

		// wall endpoint given by starting point plus direction according to starter code
		struct point2D wallEnd;
		wallEnd.px = wallStart.px + wallRay.d.px;
		wallEnd.py = wallStart.py + wallRay.d.py;

		// we are now finding intersection between two lines.
		// the light ray is defined by rayStart + lambda*rayDirection
		// so, x = rayStart.px + lambda*rayDirection.px and similarly for y
		// the (infinite) wall line is defined by (x - wallStart.px)(wallEnd.py - wallStart.py) - (y - wallStart.py)(wallEnd.px - wallStart.px) = 0
		// sub in: (rayStart.px + lambda.rayDirection.px - wallStart.px)(wallEnd.py - wallStart.py) - (rayStart.py + lambda*rayDirection.py - wallStart.py)(wallEnd.px - wallStart.px) = 0
		// collect lambda terms: lambda*(rayDirection.px*(wallEnd.py - wallStart.py) - rayDirection.py*(wallEnd.px - wallStart.px)) + (rayStart.px - wallStart.px)*(wallEnd.py - wallStart.py) - (rayStart.py - wallStart.py)*(wallEnd.px - wallStart.px) = 0
		// it is now easy to solve for lambda

		double denominator = rayDirection.px * (wallEnd.py - wallStart.py) - rayDirection.py * (wallEnd.px - wallStart.px);

		if (fabs(denominator) < TOL)
		{
			continue; // ray and line are parallel, we are NOT dividing by zero
		}

		double lambdaCandidate = -((rayStart.px - wallStart.px) * (wallEnd.py - wallStart.py) - (rayStart.py - wallStart.py) * (wallEnd.px - wallStart.px)) / denominator;

		// calculate what the intersection point would be with the lambda and see if it lies on the line segment
		struct point2D intersectionCandidate;
		intersectionCandidate.px = rayStart.px + rayDirection.px * lambdaCandidate;
		intersectionCandidate.py = rayStart.py + rayDirection.py * lambdaCandidate;

		// we already know that the point is on the line, so we just need to see if it lies in the valid x and y range
		if (lambdaCandidate > TOL && lambdaCandidate < *lambda &&
			intersectionCandidate.px <= fmax(wallStart.px, wallEnd.px) + TOL &&
			intersectionCandidate.px >= fmin(wallStart.px, wallEnd.px) - TOL &&
			intersectionCandidate.py <= fmax(wallStart.py, wallEnd.py) + TOL &&
			intersectionCandidate.py >= fmin(wallStart.py, wallEnd.py) - TOL)
		{
			// valid, find normal and update it
			// wall order is hard coded and the border is square so hardcoding works well
			switch (i)
			{
			case 0: // top
				n->px = 0;
				n->py = 1;
				break;
			case 1: // right
				n->px = -1;
				n->py = 0;
				break;
			case 2: // bottom
				n->px = 0;
				n->py = -1;
				break;
			case 3: // left
				n->px = 1;
				n->py = 0;
				break;
			default:
				fprintf(stderr, "Something has gone very wrong with the walls. They're in the walls! They're in the walls!");
			}

#ifdef __DEBUG_MODE
			fprintf(stderr,
					"WALL %d: lambda=%f hit=(%f,%f)\n",
					i, lambdaCandidate, intersectionCandidate.px, intersectionCandidate.py);
#endif

			hitWall = 1;

			// update collision point
			p->px = intersectionCandidate.px;
			p->py = intersectionCandidate.py;

			// update lambda
			*lambda = lambdaCandidate;

			// update material type
			*type = walls[i].materialType;

			// update wall colour
			*tR = walls[i].red;
			*tG = walls[i].green;
			*tB = walls[i].blue;
		}
	}

	return hitWall;
}

// returns 1 if the ray fell into a black hole
int accelerateRay(struct ray2D *ray, double integrationDistance)
{
	if (numBlackHoles == 0) return 0;

	double gravitationalConstant = 6.67430e-11;
	double c = 299792458.0;

	// make super duper sure the current direction is normalized
	normalize(&ray->d);

	struct point2D totalAcceleration;
	totalAcceleration.px = 0.0;
	totalAcceleration.py = 0.0;

	// apply acceleration from all black holes
	// nearly the exact same math as in warped rays
	for (int i = 0; i < numBlackHoles; i++)
	{
		double m = blackHoles[i].mass;

		struct point2D r;
		r.px = ray->p.px - blackHoles[i].c.px;
		r.py = ray->p.py - blackHoles[i].c.py;

		double distanceSquared = r.px * r.px + r.py * r.py;
		double d = sqrt(distanceSquared);

		// photon has reached the black hole, don't draw it anymore
		if (d <= blackHoles[i].eventHorizon)
		{
			return 1;
		}

		r.px /= d;
		r.py /= d;

		double accelerationMagnitude = -(2.0 * gravitationalConstant * m) / distanceSquared;

		totalAcceleration.px += accelerationMagnitude * r.px;
		totalAcceleration.py += accelerationMagnitude * r.py;
	}

	double projection = totalAcceleration.px * ray->d.px + totalAcceleration.py * ray->d.py;

	struct point2D accP;
	accP.px = totalAcceleration.px - projection * ray->d.px;
	accP.py = totalAcceleration.py - projection * ray->d.py;

	ray->d.px += accP.px * (integrationDistance / (c * c));
	ray->d.py += accP.py * (integrationDistance / (c * c));

	normalize(&ray->d);

	return 0;
}

void initQueue(struct rayQueue *q)
{
	q->front = 0;
	q->back = 0;
	q->count = 0;
}

int enqueue(struct rayQueue *q, struct ray2D ray, int depth, double distanceTravelled, double physicsDistanceRemaining)
{
	if (q->count >= MAX_QUEUE)
	{
		return 0; // queue full
	}

	q->rays[q->back].ray = ray;
	q->rays[q->back].depth = depth;
	q->rays[q->back].distanceTravelled = distanceTravelled;
	q->rays[q->back].physicsDistanceRemaining = physicsDistanceRemaining;

	q->back = (q->back + 1) % MAX_QUEUE;
	q->count++;

	return 1;
}

int dequeue(struct rayQueue *q, struct ray2D *ray, int *depth, double *distanceTravelled, double *physicsDistanceRemaining)
{
	if (q->count == 0)
	{
		return 0; // queue empty
	}

	*ray = q->rays[q->front].ray;
	*depth = q->rays[q->front].depth;
	*distanceTravelled = q->rays[q->front].distanceTravelled;

	if (physicsDistanceRemaining != NULL)
	{
		*physicsDistanceRemaining = q->rays[q->front].physicsDistanceRemaining;
	}

	q->front = (q->front + 1) % MAX_QUEUE;
	q->count--;

	return 1;
}

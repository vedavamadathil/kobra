#version 450

// Import bindings
#include "bindings.h"

// Import all modules
#include "../../include/types.hpp"
#include "modules/ray.glsl"
#include "modules/bbox.glsl"
#include "modules/color.glsl"
#include "modules/layouts.glsl"
#include "modules/random.glsl"
#include "modules/material.glsl"
#include "modules/primitives.glsl"
#include "modules/environment.glsl"
#include "modules/intersect.glsl"
#include "modules/bvh.glsl"

// Maximum ray depth
#define MAX_DEPTH 10

// Sample random point in triangle
float u = 0.0;
float v = 0.0;

vec3 sample_triangle(vec3 v1, vec3 v2, vec3 v3, float strata, float i)
{
	// Ignore random-ness if only 1 sample
	if (pc.samples_per_light == 1)
		return (v1 + v2 + v3) / 3.0;

	// Get random point in triangle
	vec2 uv = jitter2d(strata, i);
	if (uv.x + uv.y > 1.0)
		uv = vec2(1.0 - uv.x, 1.0 - uv.y);

	// Edge vectors
	vec3 e1 = v2 - v1;
	vec3 e2 = v3 - v1;

	vec3 p = v1 + uv.x * e1 + uv.y * e2;

	return p;
}

// Point light contribution
vec3 point_light_contr(Hit hit, Ray ray, vec3 lpos)
{
	// Shadow ray
	vec3 origin = hit.point + hit.normal * 0.001;
	Ray shadow_ray = Ray(
		origin,
		normalize(lpos - origin),
		1.0, 1.0
	);

	Hit shadow_hit = closest_object(shadow_ray);

	// Light contribution
	vec3 ldir = normalize(lpos - hit.point);

	// Light intensity
	float d = distance(lpos, hit.point);
	float intensity = 5.0/d;

	// TODO: use the actual object id
	vec3 lcolor = vec3(1.0);
	if (shadow_hit.mat.shading != SHADING_TYPE_EMISSIVE)
		lcolor = vec3(0.0);

	return lcolor * intensity
		* max(0.0, dot(hit.normal, ldir))
		* hit.mat.albedo;
}

// Area light contribution
vec3 area_light_contr(Hit hit, Ray ray, uint li)
{
	// Get light position
	float a = lights.data[li + 1].x;
	float b = lights.data[li + 1].y;
	float c = lights.data[li + 1].z;
	float d = lights.data[li + 1].w;

	uint ia = floatBitsToUint(a);
	uint ib = floatBitsToUint(b);
	uint ic = floatBitsToUint(c);
	uint id = floatBitsToUint(d);

	vec3 v1 = vertices.data[2 * ia].xyz;
	vec3 v2 = vertices.data[2 * ib].xyz;
	vec3 v3 = vertices.data[2 * ic].xyz;

	vec3 lpos = (v1 + v2 + v3) / 3.0;

	// Sample points from triangle
	vec3 total_color = vec3(0.0);

	for (int i = 0; i < pc.samples_per_light; i++) {
		// Sample point from triangle
		vec3 light_position = sample_triangle(
			v1, v2, v3,
			sqrt(pc.samples_per_light), i
		);

		total_color += point_light_contr(hit, ray, light_position);
	}

	return total_color/float(pc.samples_per_light);
}

// Direct illumination
vec3 direct_illumination(Hit hit, Ray ray)
{
	// Direct light contribution
	vec3 direct_contr = vec3(0.0);

	// Direct illumination
	for (int i = 0; i < pc.lights; i++) {
		uint light_index = floatBitsToUint(light_indices.data[i]);
		direct_contr += area_light_contr(hit, ray, light_index);
	}

	return direct_contr;
}

// Color value at from a ray
vec3 color_at(Ray ray)
{
	vec3 contribution = vec3(0.0);
	float beta = 1.0;
	float ior = 1.0;

	Ray r = ray;
	for (int i = 0; i < MAX_DEPTH; i++) {
		// Find closest object
		Hit hit = closest_object(r);

		// Special case intersection
		if (hit.object == -1 || hit.mat.shading == SHADING_TYPE_EMISSIVE) {
			contribution += hit.mat.albedo;
			break;
		}

		// Direct illumination
		vec3 direct_contr = direct_illumination(hit, r)/PI;
		contribution += beta * direct_contr;

		// Generating the new ray according to BSDF
		if (hit.mat.shading == SHADING_TYPE_DIFFUSE) {
			// Lambertian BSDF
			vec3 r_dir = random_hemi(hit.normal);
			r = Ray(
				hit.point + hit.normal * 0.001,
				r_dir, 1.0, 1.0
			);

			// Update beta
			beta *= dot(hit.normal, r_dir);

		// TODO: recdesign material interface (reflection | transmission
		// and specular class)
		} else if (hit.mat.shading == SHADING_TYPE_REFLECTION) {
			// (Perfect) Specular BSDF
			vec3 r_dir = reflect(r.direction, hit.normal);
			r = Ray(
				hit.point + hit.normal * 0.001,
				r_dir, 1.0, 1.0
			);

			// Update beta
			beta *= dot(hit.normal, r_dir);
		} else if (hit.mat.shading == SHADING_TYPE_REFRACTION) {
			// (Perfect) Transmissive BSDF
			vec3 r_dir = refract(r.direction, hit.normal, ior/hit.mat.ior);
			r = Ray(
				hit.point - hit.normal * 0.001,
				r_dir, 1.0, 1.0
			);

			// Update beta
			beta *= dot(hit.normal, r_dir);
			ior = hit.mat.ior;
		} else {
			// Assume no interaction
			break;
		}
	}

	return clamp(contribution, 0.0, 1.0);
}

void main()
{
	// Offset from space origin
	uint y0 = gl_WorkGroupID.y + pc.xoffset;
	uint x0 = gl_WorkGroupID.x + pc.yoffset;

	// Return if out of bounds
	if (y0 >= pc.height || x0 >= pc.width)
		return;

	// Get index
	uint index = y0 * pc.width + x0;

	// Set seed
	float rx = fract(sin(x0 * 1234.56789 + y0) * PHI);
	float ry = fract(sin(y0 * 9876.54321 + x0));

	// Initialiize the random seed
	random_seed = vec3(rx, ry, pc.time);

	// Accumulate color
	vec3 color = vec3(0.0);

	vec2 dimensions = vec2(pc.width, pc.height);
	for (int i = 0; i < pc.samples_per_pixel; i++) {
		// Random offset
		vec2 offset = jitter2d(
			sqrt(pc.total),
			i + pc.present
		);

		// Create the ray
		vec2 pixel = vec2(x0, y0) + offset;
		vec2 uv = pixel / dimensions;

		Ray ray = make_ray(uv,
			pc.camera_position,
			pc.camera_forward,
			pc.camera_up,
			pc.camera_right,
			pc.properties.x,
			pc.properties.y
		);

		// Light transport
		color += color_at(ray);
	}

	if (pc.accumulate > 0) {
		vec3 pcolor = cast_color(frame.pixels[index]);
		pcolor = pow(pcolor, vec3(2.2));
		color += pcolor * pc.present;
		color /= float(pc.present + pc.samples_per_pixel);
		color = pow(color, vec3(1/2.2));
	} else {
		color /= float(pc.samples_per_pixel);
		color = pow(color, vec3(1/2.2));
	}

	frame.pixels[index] = cast_color(color);
}
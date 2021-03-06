#ifndef KOBRA_RT_MESH_BINDINGS_H_
#define KOBRA_RT_MESH_BINDINGS_H_

// Number of textures
#define MAX_TEXTURES 100

// Essential buffers
const int MESH_BINDING_PIXELS		= 0;
const int MESH_BINDING_VERTICES		= 1;
const int MESH_BINDING_TRIANGLES	= 2;
const int MESH_BINDING_TRANSFORMS	= 3;
const int MESH_BINDING_BVH		= 4;
const int MESH_BINDING_MATERIALS	= 5;

// Buffers for lights
const int MESH_BINDING_LIGHTS		= 6;
const int MESH_BINDING_LIGHT_INDICES	= 7;
const int MESH_BINDING_AREA_LIGHTS	= 12;

// Samplers
const int MESH_BINDING_ALBEDOS		= 8;
const int MESH_BINDING_NORMAL_MAPS	= 9;
const int MESH_BINDING_ENVIRONMENT	= 10;

// Output
const int MESH_BINDING_OUTPUT		= 11;

// TODO: mesh roughness/bump map

#endif

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef JPH_CAPI
#if defined(JPH_GLUE_BUILD) && defined(_WIN32)
#define JPH_CAPI __declspec(dllexport)
#elif defined(JPH_GLUE_BUILD)
#define JPH_CAPI __attribute__((visibility("default")))
#else
#define JPH_CAPI
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JPH_PhysicsSystem JPH_PhysicsSystem;
typedef struct JPH_BodyInterface JPH_BodyInterface;
typedef struct JPH_NarrowPhaseQuery JPH_NarrowPhaseQuery;
typedef struct JPH_Body JPH_Body;
typedef struct JPH_Shape JPH_Shape;
typedef struct JPH_PhysicsSystemState JPH_PhysicsSystemState;

typedef uint32_t JPH_BodyID;

typedef struct JPH_Vec3
{
    float x;
    float y;
    float z;
} JPH_Vec3;

typedef struct JPH_Quat
{
    float x;
    float y;
    float z;
    float w;
} JPH_Quat;

typedef struct JPH_RayCast
{
    JPH_Vec3 origin;
    JPH_Vec3 direction;
} JPH_RayCast;

typedef struct JPH_RayCastResult
{
    JPH_BodyID bodyID;
    float fraction;
} JPH_RayCastResult;

#ifdef __cplusplus
}
#endif

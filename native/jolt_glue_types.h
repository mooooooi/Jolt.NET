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
typedef struct JPH_Constraint JPH_Constraint;
typedef struct JPH_Shape JPH_Shape;
typedef struct JPH_PhysicsSystemState JPH_PhysicsSystemState;

typedef uint32_t JPH_BodyID;

enum
{
    JPH_INVALID_BODY_ID = 0xffffffffu
};

typedef enum JPH_MotionType
{
    JPH_MotionType_Static = 0,
    JPH_MotionType_Kinematic = 1,
    JPH_MotionType_Dynamic = 2
} JPH_MotionType;

typedef enum JPH_MotionQuality
{
    JPH_MotionQuality_Discrete = 0,
    JPH_MotionQuality_LinearCast = 1
} JPH_MotionQuality;

typedef enum JPH_Activation
{
    JPH_Activation_Activate = 0,
    JPH_Activation_DontActivate = 1
} JPH_Activation;

typedef enum JPH_ConstraintKind
{
    JPH_ConstraintKind_Fixed = 1,
    JPH_ConstraintKind_Distance = 2
} JPH_ConstraintKind;

typedef enum JPH_ConstraintSpace
{
    JPH_ConstraintSpace_LocalToBodyCOM = 0,
    JPH_ConstraintSpace_WorldSpace = 1
} JPH_ConstraintSpace;

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
    uint32_t subShapeID2;
} JPH_RayCastResult;

typedef struct JPH_BodyCreationSettings
{
    const JPH_Shape* shape;
    JPH_Vec3 position;
    JPH_Quat rotation;
    JPH_Vec3 linearVelocity;
    JPH_Vec3 angularVelocity;
    uint64_t userData;
    uint32_t objectLayer;
    uint8_t motionType;
    uint8_t motionQuality;
    uint8_t isSensor;
    uint8_t allowSleeping;
    float linearDamping;
    float angularDamping;
    float maxLinearVelocity;
    float maxAngularVelocity;
    float gravityFactor;
} JPH_BodyCreationSettings;

typedef struct JPH_ConstraintCreationSettings
{
    uint8_t kind;
    uint8_t enabled;
    uint8_t space;
    uint8_t autoDetectPoint;
    uint32_t priority;
    uint32_t numVelocityStepsOverride;
    uint32_t numPositionStepsOverride;
    float minDistance;
    float maxDistance;
    uint64_t userData;
} JPH_ConstraintCreationSettings;

#ifdef __cplusplus
}
#endif

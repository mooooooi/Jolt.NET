#pragma once

#include "jolt_glue_types.h"

#ifdef __cplusplus
extern "C" {
#endif

JPH_CAPI void JPH_Init(void);
JPH_CAPI void JPH_Shutdown(void);

JPH_CAPI JPH_PhysicsSystem* JPH_PhysicsSystem_Create(
    uint32_t maxBodies,
    uint32_t numBodyMutexes,
    uint32_t maxBodyPairs,
    uint32_t maxContactConstraints);
JPH_CAPI void JPH_PhysicsSystem_Destroy(JPH_PhysicsSystem* system);
JPH_CAPI uint32_t JPH_PhysicsSystem_Update(JPH_PhysicsSystem* system, float deltaTime, int collisionSteps);

JPH_CAPI JPH_BodyInterface* JPH_PhysicsSystem_GetBodyInterface(JPH_PhysicsSystem* system);
JPH_CAPI JPH_NarrowPhaseQuery* JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(JPH_PhysicsSystem* system);

JPH_CAPI JPH_Shape* JPH_Shape_CreateSphere(float radius);
JPH_CAPI JPH_Shape* JPH_Shape_CreateBox(JPH_Vec3 halfExtent, float convexRadius);
JPH_CAPI JPH_Shape* JPH_Shape_CreateCapsule(float halfHeightOfCylinder, float radius);
JPH_CAPI void JPH_Shape_AddRef(const JPH_Shape* shape);
JPH_CAPI void JPH_Shape_Release(const JPH_Shape* shape);

JPH_CAPI JPH_BodyID JPH_BodyInterface_CreateAndAddBody(
    JPH_BodyInterface* bodyInterface,
    const JPH_BodyCreationSettings* settings,
    JPH_Activation activation);
JPH_CAPI void JPH_BodyInterface_RemoveAndDestroyBody(JPH_BodyInterface* bodyInterface, JPH_BodyID bodyID);
JPH_CAPI uint8_t JPH_BodyInterface_SetPositionAndRotation(
    JPH_BodyInterface* bodyInterface,
    JPH_BodyID bodyID,
    JPH_Vec3 position,
    JPH_Quat rotation,
    JPH_Activation activation);
JPH_CAPI uint8_t JPH_BodyInterface_GetPositionAndRotation(
    const JPH_BodyInterface* bodyInterface,
    JPH_BodyID bodyID,
    JPH_Vec3* position,
    JPH_Quat* rotation);
JPH_CAPI uint8_t JPH_BodyInterface_SetLinearAndAngularVelocity(
    JPH_BodyInterface* bodyInterface,
    JPH_BodyID bodyID,
    JPH_Vec3 linearVelocity,
    JPH_Vec3 angularVelocity);
JPH_CAPI uint8_t JPH_BodyInterface_GetLinearAndAngularVelocity(
    const JPH_BodyInterface* bodyInterface,
    JPH_BodyID bodyID,
    JPH_Vec3* linearVelocity,
    JPH_Vec3* angularVelocity);
JPH_CAPI uint8_t JPH_BodyInterface_IsAdded(const JPH_BodyInterface* bodyInterface, JPH_BodyID bodyID);

JPH_CAPI JPH_Constraint* JPH_PhysicsSystem_CreateAndAddConstraint(
    JPH_PhysicsSystem* system,
    JPH_BodyID bodyID1,
    JPH_BodyID bodyID2,
    const JPH_ConstraintCreationSettings* settings);
JPH_CAPI uint8_t JPH_PhysicsSystem_RemoveAndDestroyConstraint(JPH_PhysicsSystem* system, JPH_Constraint* constraint);

JPH_CAPI uint8_t JPH_NarrowPhaseQuery_CastRay(
    const JPH_NarrowPhaseQuery* query,
    const JPH_RayCast* ray,
    JPH_RayCastResult* hit);
JPH_CAPI uint32_t JPH_NarrowPhaseQuery_CastRayAll(
    const JPH_NarrowPhaseQuery* query,
    const JPH_RayCast* ray,
    JPH_RayCastResult* hits,
    uint32_t maxHits);

JPH_CAPI JPH_PhysicsSystemState* JPH_PhysicsSystem_SaveAlignedState(JPH_PhysicsSystem* system);
JPH_CAPI uint8_t JPH_PhysicsSystem_RestoreAlignedState(JPH_PhysicsSystem* system, const JPH_PhysicsSystemState* state);
JPH_CAPI size_t JPH_PhysicsSystemState_GetSize(const JPH_PhysicsSystemState* state);
JPH_CAPI const uint8_t* JPH_PhysicsSystemState_GetData(const JPH_PhysicsSystemState* state);
JPH_CAPI JPH_PhysicsSystemState* JPH_PhysicsSystemState_CreateFromData(const uint8_t* data, size_t size);
JPH_CAPI void JPH_PhysicsSystemState_Destroy(JPH_PhysicsSystemState* state);

#ifdef __cplusplus
}
#endif

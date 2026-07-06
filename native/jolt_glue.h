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

JPH_CAPI uint8_t JPH_NarrowPhaseQuery_CastRay(
    const JPH_NarrowPhaseQuery* query,
    const JPH_RayCast* ray,
    JPH_RayCastResult* hit);

JPH_CAPI JPH_PhysicsSystemState* JPH_PhysicsSystem_SaveAlignedState(JPH_PhysicsSystem* system);
JPH_CAPI uint8_t JPH_PhysicsSystem_RestoreAlignedState(JPH_PhysicsSystem* system, const JPH_PhysicsSystemState* state);
JPH_CAPI size_t JPH_PhysicsSystemState_GetSize(const JPH_PhysicsSystemState* state);
JPH_CAPI const uint8_t* JPH_PhysicsSystemState_GetData(const JPH_PhysicsSystemState* state);
JPH_CAPI JPH_PhysicsSystemState* JPH_PhysicsSystemState_CreateFromData(const uint8_t* data, size_t size);
JPH_CAPI void JPH_PhysicsSystemState_Destroy(JPH_PhysicsSystemState* state);

#ifdef __cplusplus
}
#endif

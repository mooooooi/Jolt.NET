#include "jolt_glue.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorder.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    constexpr JPH::BroadPhaseLayer kDefaultBroadPhaseLayer(0);
    constexpr uint32_t kTempAllocatorBytes = 10 * 1024 * 1024;
    constexpr JPH::uint kMaxPhysicsJobs = 2048;
    constexpr JPH::uint kMaxPhysicsBarriers = 8;

    std::mutex gInitMutex;
    std::atomic_uint32_t gInitCount { 0 };

    class SingleBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
    public:
        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return 1;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override
        {
            return kDefaultBroadPhaseLayer;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override
        {
            return "Default";
        }
#endif
    };

    class AllObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override
        {
            return true;
        }
    };

    class AllObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override
        {
            return true;
        }
    };

    class VectorStateRecorder final : public JPH::StateRecorder
    {
    public:
        VectorStateRecorder() = default;

        explicit VectorStateRecorder(const uint8_t *data, size_t size)
        {
            if (data != nullptr && size > 0)
                mData.assign(data, data + size);
        }

        void WriteBytes(const void *data, size_t numBytes) override
        {
            if (data == nullptr && numBytes > 0)
            {
                mFailed = true;
                return;
            }

            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            mData.insert(mData.end(), bytes, bytes + numBytes);
        }

        void ReadBytes(void *data, size_t numBytes) override
        {
            if ((data == nullptr && numBytes > 0) || mReadOffset + numBytes > mData.size())
            {
                mFailed = true;
                return;
            }

            std::memcpy(data, mData.data() + mReadOffset, numBytes);
            mReadOffset += numBytes;
        }

        bool IsEOF() const override
        {
            return mReadOffset >= mData.size();
        }

        bool IsFailed() const override
        {
            return mFailed;
        }

        const std::vector<uint8_t> &GetData() const
        {
            return mData;
        }

    private:
        std::vector<uint8_t> mData;
        size_t mReadOffset = 0;
        bool mFailed = false;
    };

    JPH::uint WorkerThreadCount()
    {
        uint32_t hardware = std::thread::hardware_concurrency();
        if (hardware <= 1)
            return 0;

        return std::min<uint32_t>(hardware - 1, 8);
    }

    void EnsureJoltInitialized()
    {
        if (gInitCount.load(std::memory_order_acquire) > 0)
            return;

        JPH_Init();
    }
}

struct JPH_PhysicsSystem
{
    SingleBroadPhaseLayerInterface broadPhaseLayerInterface;
    AllObjectVsBroadPhaseLayerFilter objectVsBroadPhaseLayerFilter;
    AllObjectLayerPairFilter objectLayerPairFilter;
    JPH::TempAllocatorImpl tempAllocator;
    JPH::JobSystemThreadPool jobSystem;
    JPH::PhysicsSystem physicsSystem;

    JPH_PhysicsSystem(uint32_t maxBodies, uint32_t numBodyMutexes, uint32_t maxBodyPairs, uint32_t maxContactConstraints)
        : tempAllocator(kTempAllocatorBytes),
          jobSystem(kMaxPhysicsJobs, kMaxPhysicsBarriers, WorkerThreadCount())
    {
        physicsSystem.Init(
            maxBodies,
            numBodyMutexes,
            maxBodyPairs,
            maxContactConstraints,
            broadPhaseLayerInterface,
            objectVsBroadPhaseLayerFilter,
            objectLayerPairFilter);
    }
};

struct JPH_PhysicsSystemState
{
    std::vector<uint8_t> data;
};

extern "C"
{
    void JPH_Init(void)
    {
        std::lock_guard<std::mutex> lock(gInitMutex);
        if (gInitCount.fetch_add(1, std::memory_order_acq_rel) == 0)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    void JPH_Shutdown(void)
    {
        std::lock_guard<std::mutex> lock(gInitMutex);
        uint32_t current = gInitCount.load(std::memory_order_acquire);
        if (current == 0)
            return;

        if (gInitCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    JPH_PhysicsSystem *JPH_PhysicsSystem_Create(uint32_t maxBodies, uint32_t numBodyMutexes, uint32_t maxBodyPairs, uint32_t maxContactConstraints)
    {
        try
        {
            EnsureJoltInitialized();
            return new JPH_PhysicsSystem(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void JPH_PhysicsSystem_Destroy(JPH_PhysicsSystem *system)
    {
        delete system;
    }

    uint32_t JPH_PhysicsSystem_Update(JPH_PhysicsSystem *system, float deltaTime, int collisionSteps)
    {
        if (system == nullptr)
            return 0xffffffffu;

        JPH::EPhysicsUpdateError error = system->physicsSystem.Update(deltaTime, collisionSteps, &system->tempAllocator, &system->jobSystem);
        return static_cast<uint32_t>(error);
    }

    JPH_BodyInterface *JPH_PhysicsSystem_GetBodyInterface(JPH_PhysicsSystem *system)
    {
        if (system == nullptr)
            return nullptr;

        return reinterpret_cast<JPH_BodyInterface *>(&system->physicsSystem.GetBodyInterface());
    }

    JPH_NarrowPhaseQuery *JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(JPH_PhysicsSystem *system)
    {
        if (system == nullptr)
            return nullptr;

        return reinterpret_cast<JPH_NarrowPhaseQuery *>(const_cast<JPH::NarrowPhaseQuery *>(&system->physicsSystem.GetNarrowPhaseQueryNoLock()));
    }

    uint8_t JPH_NarrowPhaseQuery_CastRay(const JPH_NarrowPhaseQuery *query, const JPH_RayCast *ray, JPH_RayCastResult *hit)
    {
        if (query == nullptr || ray == nullptr || hit == nullptr)
            return 0;

        const JPH::NarrowPhaseQuery *nativeQuery = reinterpret_cast<const JPH::NarrowPhaseQuery *>(query);
        JPH::RRayCast nativeRay(
            JPH::RVec3(ray->origin.x, ray->origin.y, ray->origin.z),
            JPH::Vec3(ray->direction.x, ray->direction.y, ray->direction.z));

        JPH::RayCastResult nativeHit;
        nativeHit.mFraction = hit->fraction > 0.0f ? hit->fraction : 1.0f;

        if (!nativeQuery->CastRay(nativeRay, nativeHit))
            return 0;

        hit->bodyID = nativeHit.mBodyID.GetIndexAndSequenceNumber();
        hit->fraction = nativeHit.mFraction;
        return 1;
    }

    JPH_PhysicsSystemState *JPH_PhysicsSystem_SaveAlignedState(JPH_PhysicsSystem *system)
    {
        if (system == nullptr)
            return nullptr;

        try
        {
            VectorStateRecorder recorder;
            system->physicsSystem.SaveState(recorder);

            JPH_PhysicsSystemState *state = new JPH_PhysicsSystemState();
            state->data = recorder.GetData();
            return state;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    uint8_t JPH_PhysicsSystem_RestoreAlignedState(JPH_PhysicsSystem *system, const JPH_PhysicsSystemState *state)
    {
        if (system == nullptr || state == nullptr)
            return 0;

        try
        {
            VectorStateRecorder recorder(state->data.data(), state->data.size());
            return system->physicsSystem.RestoreState(recorder) && !recorder.IsFailed() ? 1 : 0;
        }
        catch (...)
        {
            return 0;
        }
    }

    size_t JPH_PhysicsSystemState_GetSize(const JPH_PhysicsSystemState *state)
    {
        return state != nullptr ? state->data.size() : 0;
    }

    const uint8_t *JPH_PhysicsSystemState_GetData(const JPH_PhysicsSystemState *state)
    {
        if (state == nullptr || state->data.empty())
            return nullptr;

        return state->data.data();
    }

    JPH_PhysicsSystemState *JPH_PhysicsSystemState_CreateFromData(const uint8_t *data, size_t size)
    {
        if (data == nullptr && size > 0)
            return nullptr;

        JPH_PhysicsSystemState *state = new JPH_PhysicsSystemState();
        if (size > 0)
            state->data.assign(data, data + size);
        return state;
    }

    void JPH_PhysicsSystemState_Destroy(JPH_PhysicsSystemState *state)
    {
        delete state;
    }
}

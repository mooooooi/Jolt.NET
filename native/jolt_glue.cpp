#include "jolt_glue.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionQuality.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
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

    class MaskObjectLayerFilter final : public JPH::ObjectLayerFilter
    {
    public:
        explicit MaskObjectLayerFilter(uint64_t mask)
            : mMask(mask)
        {
        }

        bool ShouldCollide(JPH::ObjectLayer layer) const override
        {
            const uint32_t bit = static_cast<uint32_t>(layer);
            return bit < 64 && (mMask & (uint64_t { 1 } << bit)) != 0;
        }

    private:
        uint64_t mMask;
    };

    class ManagedObjectLayerFilter final : public JPH::ObjectLayerFilter
    {
    public:
        static const JPH_ObjectLayerFilter_Procs *sProcs;

        explicit ManagedObjectLayerFilter(void *userData)
            : mUserData(userData)
        {
        }

        bool ShouldCollide(JPH::ObjectLayer objectLayer) const override
        {
            if (sProcs != nullptr && sProcs->ShouldCollide != nullptr)
                return sProcs->ShouldCollide(mUserData, static_cast<uint32_t>(objectLayer)) != 0;

            return true;
        }

    private:
        void *mUserData;
    };

    const JPH_ObjectLayerFilter_Procs *ManagedObjectLayerFilter::sProcs = nullptr;

    class ManagedBodyFilter final : public JPH::BodyFilter
    {
    public:
        static const JPH_BodyFilter_Procs *sProcs;

        explicit ManagedBodyFilter(void *userData)
            : mUserData(userData)
        {
        }

        bool ShouldCollide(const JPH::BodyID &bodyID) const override
        {
            if (sProcs != nullptr && sProcs->ShouldCollide != nullptr)
                return sProcs->ShouldCollide(mUserData, bodyID.GetIndexAndSequenceNumber()) != 0;

            return true;
        }

        bool ShouldCollideLocked(const JPH::Body &body) const override
        {
            if (sProcs != nullptr && sProcs->ShouldCollideLocked != nullptr)
                return sProcs->ShouldCollideLocked(mUserData, reinterpret_cast<const JPH_Body *>(&body)) != 0;

            return true;
        }

    private:
        void *mUserData;
    };

    const JPH_BodyFilter_Procs *ManagedBodyFilter::sProcs = nullptr;

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

    JPH::Vec3 ToVec3(const JPH_Vec3 &value)
    {
        return JPH::Vec3(value.x, value.y, value.z);
    }

    JPH::RVec3 ToRVec3(const JPH_Vec3 &value)
    {
        return JPH::RVec3(value.x, value.y, value.z);
    }

    JPH::Quat ToQuat(const JPH_Quat &value)
    {
        return JPH::Quat(value.x, value.y, value.z, value.w);
    }

    JPH_Vec3 FromVec3(const JPH::Vec3 &value)
    {
        return { value.GetX(), value.GetY(), value.GetZ() };
    }

    JPH_Vec3 FromRVec3(const JPH::RVec3 &value)
    {
        return { static_cast<float>(value.GetX()), static_cast<float>(value.GetY()), static_cast<float>(value.GetZ()) };
    }

    JPH_Quat FromQuat(const JPH::Quat &value)
    {
        return { value.GetX(), value.GetY(), value.GetZ(), value.GetW() };
    }

    JPH::EMotionType ToMotionType(uint8_t value)
    {
        switch (static_cast<JPH_MotionType>(value))
        {
        case JPH_MotionType_Static:
            return JPH::EMotionType::Static;
        case JPH_MotionType_Kinematic:
            return JPH::EMotionType::Kinematic;
        case JPH_MotionType_Dynamic:
        default:
            return JPH::EMotionType::Dynamic;
        }
    }

    JPH::EMotionQuality ToMotionQuality(uint8_t value)
    {
        switch (static_cast<JPH_MotionQuality>(value))
        {
        case JPH_MotionQuality_LinearCast:
            return JPH::EMotionQuality::LinearCast;
        case JPH_MotionQuality_Discrete:
        default:
            return JPH::EMotionQuality::Discrete;
        }
    }

    JPH::EActivation ToActivation(JPH_Activation value)
    {
        return value == JPH_Activation_Activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    }

    JPH::EConstraintSpace ToConstraintSpace(uint8_t value)
    {
        switch (static_cast<JPH_ConstraintSpace>(value))
        {
        case JPH_ConstraintSpace_LocalToBodyCOM:
            return JPH::EConstraintSpace::LocalToBodyCOM;
        case JPH_ConstraintSpace_WorldSpace:
        default:
            return JPH::EConstraintSpace::WorldSpace;
        }
    }

    void ApplyConstraintSettings(JPH::ConstraintSettings &nativeSettings, const JPH_ConstraintCreationSettings &settings)
    {
        nativeSettings.mEnabled = settings.enabled != 0;
        nativeSettings.mConstraintPriority = settings.priority;
        nativeSettings.mNumVelocityStepsOverride = settings.numVelocityStepsOverride;
        nativeSettings.mNumPositionStepsOverride = settings.numPositionStepsOverride;
        nativeSettings.mUserData = settings.userData;
    }

    JPH_Constraint *FromConstraint(JPH::Constraint *constraint)
    {
        return reinterpret_cast<JPH_Constraint *>(constraint);
    }

    JPH::Constraint *ToConstraint(JPH_Constraint *constraint)
    {
        return reinterpret_cast<JPH::Constraint *>(constraint);
    }

    JPH::BodyID ToBodyID(JPH_BodyID value)
    {
        return value == JPH_INVALID_BODY_ID ? JPH::BodyID() : JPH::BodyID(value);
    }

    JPH_BodyID FromBodyID(const JPH::BodyID &value)
    {
        return value.GetIndexAndSequenceNumber();
    }

    void WriteRayCastResult(JPH_RayCastResult &destination, const JPH::RayCastResult &source)
    {
        destination.bodyID = FromBodyID(source.mBodyID);
        destination.fraction = source.mFraction;
        destination.subShapeID2 = source.mSubShapeID2.GetValue();
    }

    JPH::BodyInterface *ToBodyInterface(JPH_BodyInterface *bodyInterface)
    {
        return reinterpret_cast<JPH::BodyInterface *>(bodyInterface);
    }

    const JPH::BodyInterface *ToBodyInterface(const JPH_BodyInterface *bodyInterface)
    {
        return reinterpret_cast<const JPH::BodyInterface *>(bodyInterface);
    }

    const JPH::Shape *ToShape(const JPH_Shape *shape)
    {
        return reinterpret_cast<const JPH::Shape *>(shape);
    }

    const JPH::ObjectLayerFilter &ToObjectLayerFilter(const JPH_ObjectLayerFilter *filter)
    {
        static const JPH::ObjectLayerFilter defaultFilter = {};
        return filter != nullptr ? *reinterpret_cast<const JPH::ObjectLayerFilter *>(filter) : defaultFilter;
    }

    const JPH::BodyFilter &ToBodyFilter(const JPH_BodyFilter *filter)
    {
        static const JPH::BodyFilter defaultFilter = {};
        return filter != nullptr ? *reinterpret_cast<const JPH::BodyFilter *>(filter) : defaultFilter;
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

struct JPH_CharacterVirtual
{
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH_PhysicsSystem *system = nullptr;
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

    JPH_Shape *JPH_Shape_CreateSphere(float radius)
    {
        if (radius <= 0.0f)
            return nullptr;

        try
        {
            JPH::SphereShapeSettings settings(radius);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError())
                return nullptr;

            JPH::RefConst<JPH::Shape> shape = result.Get();
            shape->AddRef();
            return reinterpret_cast<JPH_Shape *>(const_cast<JPH::Shape *>(shape.GetPtr()));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    JPH_Shape *JPH_Shape_CreateBox(JPH_Vec3 halfExtent, float convexRadius)
    {
        if (halfExtent.x <= 0.0f || halfExtent.y <= 0.0f || halfExtent.z <= 0.0f || convexRadius < 0.0f)
            return nullptr;

        try
        {
            JPH::BoxShapeSettings settings(ToVec3(halfExtent), convexRadius);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError())
                return nullptr;

            JPH::RefConst<JPH::Shape> shape = result.Get();
            shape->AddRef();
            return reinterpret_cast<JPH_Shape *>(const_cast<JPH::Shape *>(shape.GetPtr()));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    JPH_Shape *JPH_Shape_CreateCapsule(float halfHeightOfCylinder, float radius)
    {
        if (halfHeightOfCylinder < 0.0f || radius <= 0.0f)
            return nullptr;

        try
        {
            JPH::CapsuleShapeSettings settings(halfHeightOfCylinder, radius);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError())
                return nullptr;

            JPH::RefConst<JPH::Shape> shape = result.Get();
            shape->AddRef();
            return reinterpret_cast<JPH_Shape *>(const_cast<JPH::Shape *>(shape.GetPtr()));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void JPH_Shape_AddRef(const JPH_Shape *shape)
    {
        if (shape != nullptr)
            ToShape(shape)->AddRef();
    }

    void JPH_Shape_Release(const JPH_Shape *shape)
    {
        if (shape != nullptr)
            ToShape(shape)->Release();
    }

    JPH_CharacterVirtual *JPH_CharacterVirtual_Create(
        JPH_PhysicsSystem *system,
        const JPH_CharacterVirtualCreationSettings *settings,
        const JPH_CharacterVirtualState *state)
    {
        if (system == nullptr || settings == nullptr || state == nullptr || settings->shape == nullptr)
            return nullptr;

        try
        {
            JPH::CharacterVirtualSettings nativeSettings;
            nativeSettings.mShape = ToShape(settings->shape);
            nativeSettings.mUp = ToVec3(settings->up).NormalizedOr(JPH::Vec3::sAxisY());
            nativeSettings.mShapeOffset = ToVec3(settings->shapeOffset);
            nativeSettings.mSupportingVolume = JPH::Plane(nativeSettings.mUp, settings->supportingVolumeConstant);
            nativeSettings.mMaxSlopeAngle = settings->maxSlopeAngle;
            nativeSettings.mMass = settings->mass;
            nativeSettings.mMaxStrength = settings->maxStrength;
            nativeSettings.mPredictiveContactDistance = settings->predictiveContactDistance;
            nativeSettings.mMaxCollisionIterations = settings->maxCollisionIterations;
            nativeSettings.mMaxConstraintIterations = settings->maxConstraintIterations;
            nativeSettings.mMinTimeRemaining = settings->minTimeRemaining;
            nativeSettings.mCollisionTolerance = settings->collisionTolerance;
            nativeSettings.mCharacterPadding = settings->characterPadding;
            nativeSettings.mMaxNumHits = settings->maxNumHits;
            nativeSettings.mHitReductionCosMaxAngle = settings->hitReductionCosMaxAngle;
            nativeSettings.mPenetrationRecoverySpeed = settings->penetrationRecoverySpeed;
            nativeSettings.mID = JPH::CharacterID(settings->characterID);

            auto result = new JPH_CharacterVirtual();
            result->system = system;
            result->character = new JPH::CharacterVirtual(
                &nativeSettings,
                ToRVec3(state->position),
                ToQuat(state->rotation),
                settings->userData,
                &system->physicsSystem);
            result->character->SynchronizeState(
                ToRVec3(state->position),
                ToQuat(state->rotation),
                ToVec3(state->linearVelocity),
                ToVec3(state->groundNormal),
                ToVec3(state->groundVelocity),
                static_cast<JPH::CharacterBase::EGroundState>(state->groundState),
                true);
            return result;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void JPH_CharacterVirtual_Destroy(JPH_CharacterVirtual *character)
    {
        delete character;
    }

    uint8_t JPH_CharacterVirtual_SetState(
        JPH_CharacterVirtual *character,
        const JPH_CharacterVirtualState *state,
        uint8_t resetContacts)
    {
        if (character == nullptr || character->character == nullptr || state == nullptr)
            return 0;

        character->character->SynchronizeState(
            ToRVec3(state->position),
            ToQuat(state->rotation),
            ToVec3(state->linearVelocity),
            ToVec3(state->groundNormal),
            ToVec3(state->groundVelocity),
            static_cast<JPH::CharacterBase::EGroundState>(state->groundState),
            resetContacts != 0);
        return 1;
    }

    uint8_t JPH_CharacterVirtual_GetState(
        const JPH_CharacterVirtual *character,
        JPH_CharacterVirtualState *state)
    {
        if (character == nullptr || character->character == nullptr || state == nullptr)
            return 0;

        const JPH::CharacterVirtual &native = *character->character;
        state->position = FromRVec3(native.GetPosition());
        state->rotation = FromQuat(native.GetRotation());
        state->linearVelocity = FromVec3(native.GetLinearVelocity());
        state->groundNormal = FromVec3(native.GetGroundNormal());
        state->groundVelocity = FromVec3(native.GetGroundVelocity());
        state->groundState = static_cast<JPH_CharacterGroundState>(native.GetGroundState());
        return 1;
    }

    uint8_t JPH_CharacterVirtual_ExtendedUpdate(
        JPH_CharacterVirtual *character,
        float deltaTime,
        JPH_Vec3 gravity,
        const JPH_CharacterVirtualUpdateSettings *settings,
        uint64_t collisionLayerMask)
    {
        if (character == nullptr || character->character == nullptr || character->system == nullptr ||
            settings == nullptr || deltaTime < 0.0f)
        {
            return 0;
        }

        JPH::CharacterVirtual::ExtendedUpdateSettings nativeSettings;
        nativeSettings.mStickToFloorStepDown = ToVec3(settings->stickToFloorStepDown);
        nativeSettings.mWalkStairsStepUp = ToVec3(settings->walkStairsStepUp);
        nativeSettings.mWalkStairsMinStepForward = settings->walkStairsMinStepForward;
        nativeSettings.mWalkStairsStepForwardTest = settings->walkStairsStepForwardTest;
        nativeSettings.mWalkStairsCosAngleForwardContact = settings->walkStairsCosAngleForwardContact;
        nativeSettings.mWalkStairsStepDownExtra = ToVec3(settings->walkStairsStepDownExtra);

        MaskObjectLayerFilter objectLayerFilter(collisionLayerMask);
        const JPH::BodyFilter bodyFilter;
        const JPH::ShapeFilter shapeFilter;
        character->character->ExtendedUpdate(
            deltaTime,
            ToVec3(gravity),
            nativeSettings,
            character->system->physicsSystem.GetDefaultBroadPhaseLayerFilter(0),
            objectLayerFilter,
            bodyFilter,
            shapeFilter,
            character->system->tempAllocator);
        return 1;
    }

    JPH_BodyID JPH_BodyInterface_CreateAndAddBody(JPH_BodyInterface *bodyInterface, const JPH_BodyCreationSettings *settings, JPH_Activation activation)
    {
        if (bodyInterface == nullptr || settings == nullptr || settings->shape == nullptr)
            return JPH_INVALID_BODY_ID;

        try
        {
            JPH::BodyCreationSettings nativeSettings(
                ToShape(settings->shape),
                ToRVec3(settings->position),
                ToQuat(settings->rotation),
                ToMotionType(settings->motionType),
                JPH::ObjectLayer(settings->objectLayer));

            nativeSettings.mLinearVelocity = ToVec3(settings->linearVelocity);
            nativeSettings.mAngularVelocity = ToVec3(settings->angularVelocity);
            nativeSettings.mUserData = settings->userData;
            nativeSettings.mMotionQuality = ToMotionQuality(settings->motionQuality);
            nativeSettings.mIsSensor = settings->isSensor != 0;
            nativeSettings.mAllowSleeping = settings->allowSleeping != 0;
            nativeSettings.mLinearDamping = settings->linearDamping;
            nativeSettings.mAngularDamping = settings->angularDamping;
            nativeSettings.mMaxLinearVelocity = settings->maxLinearVelocity;
            nativeSettings.mMaxAngularVelocity = settings->maxAngularVelocity;
            nativeSettings.mGravityFactor = settings->gravityFactor;

            JPH::BodyID bodyID = ToBodyInterface(bodyInterface)->CreateAndAddBody(nativeSettings, ToActivation(activation));
            return FromBodyID(bodyID);
        }
        catch (...)
        {
            return JPH_INVALID_BODY_ID;
        }
    }

    void JPH_BodyInterface_RemoveAndDestroyBody(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return;

        JPH::BodyID nativeBodyID = ToBodyID(bodyID);
        JPH::BodyInterface *nativeBodyInterface = ToBodyInterface(bodyInterface);
        nativeBodyInterface->RemoveBody(nativeBodyID);
        nativeBodyInterface->DestroyBody(nativeBodyID);
    }

    uint8_t JPH_BodyInterface_SetPositionAndRotation(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, JPH_Vec3 position, JPH_Quat rotation, JPH_Activation activation)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return 0;

        ToBodyInterface(bodyInterface)->SetPositionAndRotation(ToBodyID(bodyID), ToRVec3(position), ToQuat(rotation), ToActivation(activation));
        return 1;
    }

    uint8_t JPH_BodyInterface_GetPositionAndRotation(const JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, JPH_Vec3 *position, JPH_Quat *rotation)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || position == nullptr || rotation == nullptr)
            return 0;

        JPH::RVec3 nativePosition;
        JPH::Quat nativeRotation;
        ToBodyInterface(bodyInterface)->GetPositionAndRotation(ToBodyID(bodyID), nativePosition, nativeRotation);
        *position = FromRVec3(nativePosition);
        *rotation = FromQuat(nativeRotation);
        return 1;
    }

    uint8_t JPH_BodyInterface_SetLinearAndAngularVelocity(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, JPH_Vec3 linearVelocity, JPH_Vec3 angularVelocity)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return 0;

        JPH::BodyInterface *nativeBodyInterface = ToBodyInterface(bodyInterface);
        JPH::BodyID nativeBodyID = ToBodyID(bodyID);
        nativeBodyInterface->SetLinearVelocity(nativeBodyID, ToVec3(linearVelocity));
        nativeBodyInterface->SetAngularVelocity(nativeBodyID, ToVec3(angularVelocity));
        return 1;
    }

    uint8_t JPH_BodyInterface_GetLinearAndAngularVelocity(const JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, JPH_Vec3 *linearVelocity, JPH_Vec3 *angularVelocity)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || linearVelocity == nullptr || angularVelocity == nullptr)
            return 0;

        const JPH::BodyInterface *nativeBodyInterface = ToBodyInterface(bodyInterface);
        JPH::BodyID nativeBodyID = ToBodyID(bodyID);
        *linearVelocity = FromVec3(nativeBodyInterface->GetLinearVelocity(nativeBodyID));
        *angularVelocity = FromVec3(nativeBodyInterface->GetAngularVelocity(nativeBodyID));
        return 1;
    }

    void JPH_BodyInterface_AddForce(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, const JPH_Vec3 *force)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || force == nullptr)
            return;

        ToBodyInterface(bodyInterface)->AddForce(ToBodyID(bodyID), ToVec3(*force), JPH::EActivation::Activate);
    }

    void JPH_BodyInterface_AddTorque(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, const JPH_Vec3 *torque)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || torque == nullptr)
            return;

        ToBodyInterface(bodyInterface)->AddTorque(ToBodyID(bodyID), ToVec3(*torque), JPH::EActivation::Activate);
    }

    void JPH_BodyInterface_AddForceAndTorque(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, const JPH_Vec3 *force, const JPH_Vec3 *torque)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || force == nullptr || torque == nullptr)
            return;

        ToBodyInterface(bodyInterface)->AddForceAndTorque(ToBodyID(bodyID), ToVec3(*force), ToVec3(*torque), JPH::EActivation::Activate);
    }

    void JPH_BodyInterface_AddImpulse(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID, const JPH_Vec3 *impulse)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID || impulse == nullptr)
            return;

        ToBodyInterface(bodyInterface)->AddImpulse(ToBodyID(bodyID), ToVec3(*impulse));
    }

    void JPH_BodyInterface_ActivateBody(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return;

        ToBodyInterface(bodyInterface)->ActivateBody(ToBodyID(bodyID));
    }

    void JPH_BodyInterface_DeactivateBody(JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return;

        ToBodyInterface(bodyInterface)->DeactivateBody(ToBodyID(bodyID));
    }

    uint8_t JPH_BodyInterface_IsAdded(const JPH_BodyInterface *bodyInterface, JPH_BodyID bodyID)
    {
        if (bodyInterface == nullptr || bodyID == JPH_INVALID_BODY_ID)
            return 0;

        return ToBodyInterface(bodyInterface)->IsAdded(ToBodyID(bodyID)) ? 1 : 0;
    }

    JPH_BodyID JPH_Body_GetID(const JPH_Body *body)
    {
        if (body == nullptr)
            return JPH_INVALID_BODY_ID;

        return reinterpret_cast<const JPH::Body *>(body)->GetID().GetIndexAndSequenceNumber();
    }

    void JPH_ObjectLayerFilter_SetProcs(const JPH_ObjectLayerFilter_Procs *procs)
    {
        ManagedObjectLayerFilter::sProcs = procs;
    }

    JPH_ObjectLayerFilter *JPH_ObjectLayerFilter_Create(void *userData)
    {
        try
        {
            return reinterpret_cast<JPH_ObjectLayerFilter *>(new ManagedObjectLayerFilter(userData));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void JPH_ObjectLayerFilter_Destroy(JPH_ObjectLayerFilter *filter)
    {
        delete reinterpret_cast<ManagedObjectLayerFilter *>(filter);
    }

    void JPH_BodyFilter_SetProcs(const JPH_BodyFilter_Procs *procs)
    {
        ManagedBodyFilter::sProcs = procs;
    }

    JPH_BodyFilter *JPH_BodyFilter_Create(void *userData)
    {
        try
        {
            return reinterpret_cast<JPH_BodyFilter *>(new ManagedBodyFilter(userData));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void JPH_BodyFilter_Destroy(JPH_BodyFilter *filter)
    {
        delete reinterpret_cast<ManagedBodyFilter *>(filter);
    }

    JPH_Constraint *JPH_PhysicsSystem_CreateAndAddConstraint(JPH_PhysicsSystem *system, JPH_BodyID bodyID1, JPH_BodyID bodyID2, const JPH_ConstraintCreationSettings *settings)
    {
        if (system == nullptr || settings == nullptr || bodyID1 == JPH_INVALID_BODY_ID || bodyID2 == JPH_INVALID_BODY_ID || bodyID1 == bodyID2)
            return nullptr;

        try
        {
            JPH::BodyInterface &bodyInterface = system->physicsSystem.GetBodyInterface();
            JPH::BodyID nativeBodyID1 = ToBodyID(bodyID1);
            JPH::BodyID nativeBodyID2 = ToBodyID(bodyID2);
            JPH::TwoBodyConstraint *constraint = nullptr;

            switch (static_cast<JPH_ConstraintKind>(settings->kind))
            {
            case JPH_ConstraintKind_Fixed:
            {
                JPH::FixedConstraintSettings nativeSettings;
                ApplyConstraintSettings(nativeSettings, *settings);
                nativeSettings.mSpace = ToConstraintSpace(settings->space);
                nativeSettings.mAutoDetectPoint = settings->autoDetectPoint != 0;
                constraint = bodyInterface.CreateConstraint(&nativeSettings, nativeBodyID1, nativeBodyID2);
                break;
            }
            case JPH_ConstraintKind_Distance:
            {
                JPH::DistanceConstraintSettings nativeSettings;
                ApplyConstraintSettings(nativeSettings, *settings);
                nativeSettings.mSpace = ToConstraintSpace(settings->space);
                nativeSettings.mMinDistance = settings->minDistance;
                nativeSettings.mMaxDistance = settings->maxDistance;
                constraint = bodyInterface.CreateConstraint(&nativeSettings, nativeBodyID1, nativeBodyID2);
                break;
            }
            default:
                return nullptr;
            }

            if (constraint == nullptr)
                return nullptr;

            system->physicsSystem.AddConstraint(constraint);
            return FromConstraint(constraint);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    uint8_t JPH_PhysicsSystem_RemoveAndDestroyConstraint(JPH_PhysicsSystem *system, JPH_Constraint *constraint)
    {
        if (system == nullptr || constraint == nullptr)
            return 0;

        system->physicsSystem.RemoveConstraint(ToConstraint(constraint));
        return 1;
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

        WriteRayCastResult(*hit, nativeHit);
        return 1;
    }

    uint8_t JPH_NarrowPhaseQuery_CastRayFiltered(
        const JPH_NarrowPhaseQuery *query,
        const JPH_RayCast *ray,
        JPH_RayCastResult *hit,
        const JPH_ObjectLayerFilter *objectLayerFilter,
        const JPH_BodyFilter *bodyFilter)
    {
        if (query == nullptr || ray == nullptr || hit == nullptr)
            return 0;

        const JPH::NarrowPhaseQuery *nativeQuery = reinterpret_cast<const JPH::NarrowPhaseQuery *>(query);
        JPH::RRayCast nativeRay(
            JPH::RVec3(ray->origin.x, ray->origin.y, ray->origin.z),
            JPH::Vec3(ray->direction.x, ray->direction.y, ray->direction.z));

        JPH::RayCastResult nativeHit;
        nativeHit.mFraction = hit->fraction > 0.0f ? hit->fraction : 1.0f;

        if (!nativeQuery->CastRay(nativeRay, nativeHit, {}, ToObjectLayerFilter(objectLayerFilter), ToBodyFilter(bodyFilter)))
            return 0;

        WriteRayCastResult(*hit, nativeHit);
        return 1;
    }

    uint32_t JPH_NarrowPhaseQuery_CastRayAll(
        const JPH_NarrowPhaseQuery *query,
        const JPH_RayCast *ray,
        JPH_RayCastResult *hits,
        uint32_t maxHits)
    {
        if (query == nullptr || ray == nullptr || (hits == nullptr && maxHits > 0))
            return 0;

        try
        {
            const JPH::NarrowPhaseQuery *nativeQuery = reinterpret_cast<const JPH::NarrowPhaseQuery *>(query);
            JPH::RRayCast nativeRay(
                JPH::RVec3(ray->origin.x, ray->origin.y, ray->origin.z),
                JPH::Vec3(ray->direction.x, ray->direction.y, ray->direction.z));

            JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
            nativeQuery->CastRay(nativeRay, JPH::RayCastSettings(), collector);
            collector.Sort();

            uint32_t totalCount = static_cast<uint32_t>(collector.mHits.size());
            if (hits == nullptr || maxHits == 0)
                return totalCount;

            uint32_t count = static_cast<uint32_t>(std::min<size_t>(collector.mHits.size(), maxHits));
            for (uint32_t i = 0; i < count; ++i)
                WriteRayCastResult(hits[i], collector.mHits[i]);

            return count;
        }
        catch (...)
        {
            return 0;
        }
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

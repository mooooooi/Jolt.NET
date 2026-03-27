# Jolt.NET 代码库速查笔记

> 由 AI 探索蒸馏，供团队成员快速上手参考。  
> 更新日期：2026-03-19

---

## 目录结构

```
Jolt.NET/
├── Jolt.NET.csproj / .sln     # C# .NET 9.0 项目（当前仅占位 Hello World）
├── Program.cs
├── lib/
│   ├── JoltPhysics/           # Jolt Physics C++ 核心库（上游）
│   │   ├── Jolt/              # 引擎源码
│   │   ├── Samples/           # 官方示例
│   │   ├── UnitTests/
│   │   ├── Build/             # CMake 配置
│   │   └── Docs/
│   └── joltc/                 # C API 绑定层（本项目扩展点）
│       ├── include/joltc.h    # C API 头文件（所有对外暴露接口在此）
│       └── src/joltc.cpp      # C API 实现
├── build_osx/ build_xcode/    # 构建产物
└── clangsharp.rsp             # ClangSharp P/Invoke 生成配置
```

**关键原则**：C# 层目前无任何 Jolt 绑定，所有扩展应在 `lib/joltc/` 层完成，不改动 `lib/JoltPhysics/Jolt/` 核心。

---

## 核心数学类型

| 类型 | 头文件 | 精度 | 用途 |
|------|--------|------|------|
| `Vec3` | `Math/Vec3.h` | float (SIMD 4分量) | 计算用向量 |
| `Float3` | `Math/Float3.h` | float (3分量) | 存储/序列化用 |
| `DVec3` | `Math/DVec3.h` | double (SIMD) | 双精度计算 |
| `Double3` | `Math/Double3.h` | double (3分量) | 双精度存储 |
| `RVec3` | `Math/Real.h` | 可配置 | 实际位置类型（见下） |
| `Quat` | `Math/Quat.h` | float (Vec4) | 旋转 |
| `Float4` | `Math/Float4.h` | float (4分量) | Quat 存储 |

### RVec3 / Real 精度切换

```cpp
// Math/Real.h
#ifdef JPH_DOUBLE_PRECISION
    using Real   = double;
    using Real3  = Double3;
    using RVec3  = DVec3;    // 双精度模式
#else
    using Real   = float;
    using Real3  = Float3;
    using RVec3  = Vec3;     // 单精度模式（默认）
#endif
```

编译宏 `JPH_DOUBLE_PRECISION` 决定位置精度，需与项目编译选项一致。

---

## 刚体（Body）关键字段

**头文件**：`Physics/Body/Body.h`

```cpp
class Body {
    RVec3  mPosition;           // 质心位置（COM）
    Quat   mRotation;           // 旋转
    // 动态体另有 MotionProperties*
};
```

**MotionProperties**（`Physics/Body/MotionProperties.h`）：

```cpp
Vec3   mLinearVelocity;
Vec3   mAngularVelocity;
Float3 mForce;
Float3 mTorque;
```

**BodyInterface** 常用接口（`Physics/Body/BodyInterface.h`）：

```cpp
GetPosition / SetPosition
GetRotation / SetRotation
GetPositionAndRotation / SetPositionAndRotation
GetLinearVelocity / GetAngularVelocity
SetLinearAndAngularVelocity
SetPositionRotationAndVelocity
GetWorldTransform / GetCenterOfMassTransform
```

---

## 状态快照（Snapshot）系统

### 数据结构层级（`Physics/Snapshot/SnapshotStates.h`）

```
PhysicsSystemState
├── flags              EStateRecorderState
├── global             GlobalState { previousStepDeltaTime, gravity }
├── bodies[]           BlobArray<BodyState>
│   └── BodyState
│       ├── id         BodyID
│       ├── isActive   bool
│       ├── position   Float3
│       ├── rotation   Float4
│       └── motionProperties  MotionPropertiesState
│           ├── linearVelocity  Float3
│           ├── angularVelocity Float3
│           ├── force           Float3
│           ├── torque          Float3
│           ├── sleepTestSpheres Sphere[3]
│           └── sleepTestTimer  float
└── contacts           ContactConstraintState
    └── manifold       ManifoldCacheState
        ├── bodyPairs[] BodyPairKeyValueState
        │   └── CachedBodyPairState
        │       ├── deltaPosition  Float3
        │       ├── deltaRotation  Float3
        │       └── manifolds[]    ManifoldKeyValueState
        │           └── CachedManifoldState
        │               ├── contactNormal Float3
        │               └── contactPoints[] CachedContactPointState
        └── ccdManifolds[]
```

### BlobBuilder（`Physics/Snapshot/BlobBuilder.h`）

内存对齐的紧凑 blob 分配器，通过相对偏移指针（`BlobPtr<T>` / `BlobArray<T>`）实现零拷贝序列化：

- `BlobArray<T>` — 带长度的偏移指针数组（非裸指针，可直接 memcpy 整个 blob）
- `BlobBuilder::ConstructRoot<T>()` — 分配 blob 根节点
- `BlobBuilder::Allocate(BlobArray<T>&, length)` — 为数组分配空间并 patch 偏移

### StateRecorder（`Physics/StateRecorder.h`）

流式序列化接口，继承 `StreamIn` / `StreamOut`：

```cpp
enum class EStateRecorderState : uint8 {
    None        = 0,
    Global      = 1,
    Bodies      = 2,
    Contacts    = 4,
    Constraints = 8,
    All         = 15
};
```

适合保存/恢复完整仿真状态（含约束），也支持验证模式（确定性调试）。

---

## C API 层（joltc）

**头文件**：`lib/joltc/include/joltc.h`  
**实现**：`lib/joltc/src/joltc.cpp`

### 状态相关 API

```c
// 流式保存/恢复（StateRecorder 方案）
JPH_PhysicsSystem_SaveState(system, stream, flags, filter);
JPH_PhysicsSystem_RestoreState(system, stream, filter);

// 对齐 blob 快照（推荐，内存紧凑）
JPH_BlobBuilder* JPH_PhysicsSystem_SaveAlignedState(system, flags, filter);
bool             JPH_PhysicsSystem_RestoreAlignedState(system, buffer, bufferLength);

// BlobBuilder 辅助
uint32_t JPH_BlobBuilder_GetRequiredByteCount(builder);
void     JPH_BlobBuilder_Flush(builder, outBuffer, bufferLength);
void     JPH_BlobBuilder_Destroy(builder);

// StateRecorder 辅助
JPH_StateRecorderImpl* JPH_StateRecorderImpl_Create();
void JPH_StateRecorderImpl_Rewind(recorder);
int  JPH_StateRecorderImpl_GetDataSize(recorder);
void JPH_StateRecorderImpl_WriteBytes(recorder, data, numBytes);
void JPH_StateRecorderImpl_ReadBytes(recorder, data, numBytes);
```

### C API 中的状态结构体

```c
struct JPH_BodyState {
    JPH_BodyID  id;
    bool        isActive;
    JPH_Vec3    position;           // float x,y,z
    JPH_Quat    rotation;           // float x,y,z,w
    JPH_MotionPropertiesState motionProperties;
};

struct JPH_MotionPropertiesState {
    JPH_Vec3    linearVelocity;
    JPH_Vec3    angularVelocity;
    JPH_Vec3    force;
    JPH_Vec3    torque;
    JPH_Sphere  sleepTestSpheres[3];
    float       sleepTestTimer;
    bool        allowSleeping;
};
```

---

## 网络同步量化方案（规划中）

详见 [量化执行计划](.cursor/plans/jolt_state_quantization_294e6a5b.plan.md)。

### 精度估算（×1024 比例因子）

| 字段 | 存储类型 | 精度 | 有效范围 |
|------|----------|------|----------|
| position | `int32 × 1024` | ~1mm | ±2,000,000 m |
| rotation (Quat) | `int16 × 32767` | ~0.003% | [-1, 1]，normalize 后恢复 |
| linearVelocity | `int16 × 1024` | ~1mm/s | ±32 m/s |
| angularVelocity | `int16 × 1024` | ~1mrad/s | ±32 rad/s |

量化后每个刚体约 **44 字节**（原始 `JPH_BodyState` 约 80 字节）。

### 仅改动两个文件

- `lib/joltc/include/joltc.h`：新增 `JPH_QuantizedBodyState`、`JPH_QuantizedStateHeader` 及三个 API 声明
- `lib/joltc/src/joltc.cpp`：实现 Save / Restore / Free 逻辑

---

## 已知限制 / 注意事项

1. **跨平台确定性**：Jolt Physics 在相同平台/编译器下确定性有保证，但跨架构（ARM vs x86）不保证。
2. **C# 绑定**：当前 `Jolt.NET` 无任何 Jolt 绑定，若需 C# 调用须额外写 P/Invoke 或使用 ClangSharp（见 `clangsharp.rsp`）。
3. **双精度模式**：开启 `JPH_DOUBLE_PRECISION` 后 `BodyState.position` 为 `Double3`，量化方案需相应调整。
4. **Contact 状态**：接触法线、lambda 值同样为 float，网络同步通常只传 body 状态并让接触每步重算，不传 contacts。
5. **休眠球体**：`sleepTestSpheres` 内含 Float3 center + float radius，精度要求低，可单独用低精度量化或直接跳过（接收端重算）。

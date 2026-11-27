# PlayerTest 持枪守卫

## 试玩

打开 `/Game/Maps/PlayerTest`，点击 Play。沿用关卡原有的 `BP_PlayerCharacter`、玩家输入、跟随相机和手枪拾取物。在 World Outliner 展开 **Guard Test**，可定位出生点与守卫之间的 L 形掩体、墙北侧缺口、三个巡逻点和导航体积。

1. 在墙后等守卫转身。守卫使用与玩家相同的 Kenney 角色，双手持枪的朝向指向视野正前方。从背面过缺口可以避开注意。
2. 在守卫前方短暂探头。头顶的 **?**、黄色条和百分比表示怀疑值，迅速再探头会从保留值继续增长。
3. 持续暴露至 **!**。细黄线跟踪，粗红线锁定。红线出现后横移可避开子弹。站定时每枪扣 20 HP。在 PIE 的 Outliner 中选择实际受控玩家，查看 **GuardTestHealth → Health**。
4. 回到全高墙后，从另一端绕走。失去视线 0.8 秒后，守卫显示 **SEARCH**，走向最后目击点，左右张望 3 秒，再回原路线。开启守卫实例的 **Debug** 可显示记忆点、状态和命中日志。
5. 在 PIE 中选中守卫实例，点击 **Debug Kill** 可中断其瞄准。重新 Play 会恢复初始生命和路线。

## 摆放第二名守卫

从 Content Browser 的 **BluePrint → RangedGuard** 拖入 **BP_RangedGuard**。在可行走地面放好 Target Point，选中守卫，在 **Details → Guard → Patrol → Patrol Points** 按访问顺序添加数组项，用吸管逐项指定。空数组在出生位置值守，单点路线抵达后值守。复制实例后可重新指定整条路线。每个实例各自保存怀疑、目标下标、目击点和攻击阶段。

在新地图中用 **Nav Mesh Bounds Volume** 包住出生位置、所有目标点和绕墙路径，执行 **Build → Build Paths**。按 **P** 检查连通的绿色区域。掩体应阻挡 Pawn 和 Visibility。守卫蓝图默认使用新控制器，Auto Possess AI 为 Placed in World or Spawned。

## 资产与调参

| 位置 | 职责 |
|---|---|
| `/Game/BluePrint/RangedGuard/BP_RangedGuard` | 独立 Character 蓝图。枪口 Muzzle、可见的朝向、状态显示、预告与命中表现、实例参数 |
| `/Game/BluePrint/RangedGuard/BP_RangedGuardController` | 独立 AIController 蓝图。Guard Perception 组件的 AI Sight 配置默认感知半角 45°、视距 1200 cm、丢失视距 1400 cm |
| `Source/TopDownPrototype/RangedGuard.h` 与 `RangedGuard.cpp` | 两个蓝图的原生父类。OnPossess 初始化，Sensed 接收 AI Sight，Tick 复核视角及遮挡，ChangeGuardState 管理转换，OnMoveCompleted 核对请求，Attack 处理单发，ShotFeedback 显示反馈 |
| `Source/TopDownPrototype/TopDownPrototype.Build.cs` | 现有游戏模块引入 AI 与导航依赖 |
| `Content/Maps/PlayerTest.umap` | 一名新守卫、Patrol A/B/C、两段 L 形墙、缺口另一侧的墙、导航体积及已构建的 Recast 导航 |

守卫复用了已有红黄材质。旧的 BP_PatrolGuard、BP_GuardAIController 和黑板均独立于新守卫的决策。伤害走 UE ApplyDamage，测试玩家在运行时获得一个共享的 GuardTestHealth 组件，多个守卫打中同一名玩家时扣同一份生命。

| 新守卫实例的 Details 分类 | 默认值 |
|---|---|
| Guard → Movement | Patrol Speed 160 cm/s，Search Speed 240 cm/s，Arrival Radius 60 cm，Patrol Wait 1 s，Search Timeout 6 s，Look Duration 3 s |
| Guard → Recognition | Suspicion Gain 60 点/s，Suspicion Decay 30 点/s，Memory Hold 0.5 s，Search Delay 0.8 s |
| Guard → Attack | Tracking Time 0.8 s，Lock Time 0.4 s，Cooldown 0.6 s，Shot Range 1600 cm，Shot Damage 20 HP |
| Vitals | 守卫默认 100 HP；玩家测试生命也从 100 HP 开始 |

任务书的阶段表与验收段落给出了两组预告时间。本版统一使用阶段表的 0.8 + 0.4 秒，每枪预告 1.2 秒，持续可见时理论枪间隔 1.8 秒。控制器每帧按游戏时间推进，锁定与开枪分属两次更新。

## UE 5.8 PIE 验收

| 项目 | 结果 | 观察 |
|---|---|---|
| T01 正背面与首枪 | 通过 | 背面维持 Patrol。正面从 Suspicious 到 Combat 用时 1.667 秒，再过 1.200 秒首枪扣血。 |
| T02 累积暴露 | 通过 | 露头 0.8 秒采到 46 点，躲 0.4 秒仍为 46，重现后 1 秒内确认。另一次露头后躲 2.2 秒回到 Patrol、怀疑归零。 |
| T03 单发与横移 | 通过 | 站定血量 100 → 80 → 60，枪间隔 1.800 秒。另一次锁定后横移，保持 Combat 且血量 60 → 60。 |
| T04 退墙与重现 | 通过 | 墙后 0.25 秒时预告线已隐藏，仍为 Combat。重新出现 0.95 秒后仍未扣血。 |
| T05 搜索 | 通过 | 退墙后约 0.82 秒进入 Search，记忆点位移 0 cm。守卫移动约 340 cm，张望后回 Patrol；搜索中重现可直接返回 Combat。 |
| T06 边界路线 | 通过 | 空路线守卫站定且能识别玩家。单点守卫抵达后值守。将目击点放在导航边界外的 X=3150 cm，守卫原地进入 Search，随后归队。 |
| T07 状态中断 | 通过 | 巡逻实走 A → B → C → A；回到 A 等待时进入战斗，旧等待结束后实测速度仍为 0。搜索移动中重现切回 Combat。锁定中杀死守卫，血量归零、线消失、玩家血量保持 60。锁定中杀死玩家，守卫解除战斗并收线。 |
| T08 实例与重开 | 通过 | 甲战斗期间，乙保持 Patrol、怀疑 0、生命 100。重新 PIE 后玩家初始生命 100，仍由原玩家控制器接管 BP_PlayerCharacter。 |
| T09 枪口被挡 | 通过 | 眼睛能看见玩家；将枪口移到墙另一侧，首个命中为 StaticMeshActor_1，玩家血量 60 → 60。 |
| T10 可躲性 | 通过（机械与画面验证） | 关闭 Debug 后，向实际玩家角色施加正常移动输入，横移约 137 cm，成功躲过已锁定的一枪。另一次有渲染的 PIE 截图可见粗红线。真人首次游玩的可理解性尚未评估。 |

本机实测记录在 `Saved/Logs/GuardCombatAcceptance.log`、`Saved/Logs/GuardPIE.log`，锁定画面在 `Saved/Screenshots/WindowsEditor/GuardLocked00000.png`。

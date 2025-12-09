## 调试器高级功能规划

### 观测与追踪
- 指令 Trace 与寄存器 Diff；开启/关闭与保存到文件。
- `record`/`replay` 基础框架，循环缓冲记录最近 N 步。

### 硬件信号观测与表达式支持
- 目标：在暂停或单步时，查询任意硬件信号当前值，并在调试表达式中与寄存器、立即数混合运算。
- 调用方式：
  - 查看信号：`info s [FILTER]`，支持前缀/通配过滤；例如 `info s exu.` 或 `info s exu.alu_result`。
  - 表达式中的信号：使用 `@hier.path` 语法，如 `p @exu.alu_result + $a0`、`p/x @idu.br_taken`。
  - 断点/监视点条件中可用：`b *ADDR if (@lsu.in.valid && @wbu.rf_wen)`、`watch @exu.alu_result`。

实现方案（DPI 注册 + Tap 机制）
- 新增 SystemVerilog DPI 接口：
  - `import "DPI-C" function void npc_signal_register(input string name, input int width, input int id);`
  - `import "DPI-C" function void npc_signal_update(input int id, input int value);`
- 在 Chisel 侧提供 `SignalTap` 黑盒：
  - `class SignalTap(name: String, width: Int) extends BlackBox`，IO：`clock/reset/in(UInt(width.W))`。
  - 生成资源 `/minirv/SignalDPI.v`，在 `initial` 调用 `npc_signal_register(name, width, id)`；在 `always @(posedge clock)` 调用 `npc_signal_update(id, in)`。
  - 通过 `suggestName` 和 `dontTouch` 确保信号名稳定可见；为关键信号统一命名：`exu.alu_result`、`idu.br_taken`、`lsu.in.valid`、`wbu.rf_wdata`、`ifu.pc` 等。
- 自动化挂载策略：
  - 在 `CpuTop` 与各流水级模块内，对需要观测的 `Wire/Reg/IO` 添加 `SignalTap`；提供辅助方法 `DebugSignals.tap(signal, "hier.path")`。
  - 最小集合：各级 PC、译码类型、ALU 结果、访存地址与数据、WB 写回相关、分支相关。
  - 可扩展：开发者在任意模块调用 `tap` 即可把信号暴露到调试器。

C++ 调试器侧改造
- 新增 `SignalRegistry`（无锁读，多线程安全）：
  - `void register(std::string name, int width, int id)`；`void update(int id, uint32_t value)`；`bool get(std::string_view name, uint32_t& value)`。
  - 支持层级名检索与前缀过滤，提供 `vector<pair<string,uint32_t>> list(prefix)`。
- 表达式解析器扩展：
  - 增加 Token `TK_SIG`（正则 `@[A-Za-z0-9_\.]+`），在求值时通过 `SignalRegistry::get(name)` 取当前值。
  - 保持与现有 `$reg`、数字、解引用 `*EXPR` 一致的优先级与错误处理。
- CLI 扩展：
  - `info s [FILTER]`：列出信号与当前值；无过滤则打印已注册信号的摘要（限制条数并提示使用过滤）。
  - `p`/`b`/`w` 命令无需改动，只需表达式支持 `@信号`。

性能与时序保证
- `SignalTap` 在 `posedge` 更新一次，保证与寄存器时序一致；组合信号建议在模块内以 `RegNext` 锁存后再观测，避免竞态。
- DPI 调用数量控制：
  - 仅在仿真时启用，提供 `SimTop.io.debug_enable`，暂停时置 `1` 以刷新一次；连续运行阶段可选择关闭以降低开销。
  - 预留批量更新接口 `npc_signal_update_batch(id[], value[])`，后续优化为矢量化更新。

命名与可维护性约定
- 层级名以模块层次为前缀，使用点分：`ifu.pc`、`idu.out.bits.inst`、`exu.alu_result`、`lsu.memReq.addr`、`wbu.rf_wdata`。
- 对需要稳定暴露的信号统一 `suggestName` 并 `dontTouch`；避免后端优化导致名变更。
- 提供 `docs/signal-map.md`（后续）列出已挂载信号清单与含义。

集成与构建
- Scala：新增 `minirv.scala.common.SignalTap.scala` 与工具方法；在 `CpuTop.scala` 中挂载核心信号。
- 资源：新增 `/minirv/SignalDPI.v`，与现有 `MemoryDPI.v/RegisterDPI.v` 同目录管理。
- C++：新增 `minirv/cpp/signal.h/.cpp`，实现注册、更新与查询；在 `expr.cpp` 引入查询接口。
- 仿真：`sim_main.cpp` 可选择在暂停点打印关键信号，或保持现状仅用于波形。

测试与验证
- 单元测试：
  - 解析：`p @exu.alu_result + $a0`、逻辑运算与括号、错误名/未注册名报错。
  - 列表：`info s exu.` 输出集合与值，过滤正确。
  - 监视点：`watch @wbu.rf_wdata` 在写回变化时命中。
- 端到端：运行 `sum.bin`，在若干断点处检查 `@ifu.pc`、`@wbu.rf_waddr/wdata` 与寄存器值一致。

后续增量
- 支持位选语法：`@exu.alu_result[31:16]` 与 `@sig[bit]`。
- 支持结构信号序列化显示（Bundle/Vec）；`info s --tree exu`。
- 批量注册工具：编译期生成 `signal-map.json`，运行时加载自动 Tap。

### 性能与工程化
- 内存读取按需（DPI 逐字/逐块），避免每次暂停拷贝 128MB。
- 统一命令与错误输出格式；帮助系统完善。
- 单元测试覆盖表达式解析、监视点、断点与输出。

### 集成关系
- 通过 `abstract-machine/scripts/platform/npc.mk` 的 `NPC_PROGRAM_PATH` 注入程序镜像，无需变更现有构建链路。
- 参考 RISC-V/QEMU 的 GDB 流程（`-s -S`），确保对接外部调试器一致性。

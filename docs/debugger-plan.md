## 调试器高级功能规划

### 断点系统
- 支持 `b *ADDR` 设置 PC 断点，`delete` 删除，`info b` 列出。
- 条件断点：`b *ADDR if EXPR`，在表达式满足时命中。

### 反汇编
- 命令 `disas ADDR [N]`，显示从地址起的 N 条指令。
- 实现方案：优先解析 `IMAGE.elf` 的 `objdump -d`；备选集成轻量 RV32I 解码器。

### 写访问能力
- `set $a0 = 0x1234` 修改寄存器。
- `set *(0x80000000){b|h|w} = DATA` 修改内存，支持字节/半字/字宽度。

### 更丰富的 `x`/`p`
- `x/N{fmt}{size} EXPR`（如 `x/8xw`、`x/16xb`），选择数量、格式、宽度。
- `p` 支持类型与宽度，兼容 GDB 风格。

### 运行控制
- `until *ADDR` 运行到某地址。
- `finish` 直到当前函数返回（基于调用约定最简推断）。

### 观测与追踪
- 指令 Trace 与寄存器 Diff；开启/关闭与保存到文件。
- `record`/`replay` 基础框架，循环缓冲记录最近 N 步。

### 性能与工程化
- 内存读取按需（DPI 逐字/逐块），避免每次暂停拷贝 128MB。
- 统一命令与错误输出格式；帮助系统完善。
- 单元测试覆盖表达式解析、监视点、断点与输出。

### 集成关系
- 通过 `abstract-machine/scripts/platform/npc.mk` 的 `NPC_PROGRAM_PATH` 注入程序镜像，无需变更现有构建链路。
- 参考 RISC-V/QEMU 的 GDB 流程（`-s -S`），确保对接外部调试器一致性。


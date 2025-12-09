# ysyx-riscv-cpu

该项目实现了用于一生一芯项目的简单cpu处理器，实现了简单五级流水

## 调试器功能简介

调试器提供交互式 CLI，用于控制仿真、设置断点/监视点、检查寄存器与内存、查看反汇编与指令跟踪、监控硬件信号等，便于定位问题与验证设计。

- 执行控制
  - `si [N]`：单步（默认 1 步），可一次前进 N 条指令
  - `c`/`continue`：继续运行直到断点/监视点/退出条件触发
  - `until *ADDR`：运行到指定地址（临时断点，命中后自动删除）

- 断点（Breakpoint）
  - `b *ADDR [if EXPR]`：在 PC 地址处设置断点，可附加条件表达式
  - `b if EXPR`：仅按条件（不绑定具体地址）触发的断点
  - `d ID`/`delete ID`：删除指定断点；`info b`：列出全部断点

- 监视点（Watchpoint）
  - `w EXPR`：设置表达式监视点，表达式值变化时暂停并打印前/后值
  - `d ID`：删除监视点；`info w`：列出全部监视点

- 寄存器与内存检查
  - `info r`：列出全部寄存器；`info r <reg>`：查看单个寄存器（支持 `pc` 与通用寄存器名）
  - `x/N{fmt}{size} EXPR`：按地址/表达式查看内存快照（`fmt`: `x|d|u|t`；`size`: `b|h|w`）
  - `p/[fmt][size] EXPR`：计算并打印表达式结果，支持解引用 `*EXPR`
  - 表达式支持：`$pc`、`$reg`、`@signal`、常量与算术/逻辑运算

- 反汇编与指令跟踪
  - `disas [ADDR] [N]`：从给定地址打印 N 行反汇编，自动关联符号信息
  - `trace on|off`：开启/关闭指令跟踪；`trace itrace --size N --file PATH` 配置容量与日志文件
  - 运行时记录最近指令与反汇编行，退出时自动刷新日志（默认 `npc/build/trace/itrace.log`）

- 信号监控
  - `info s [FILTER]`：列举匹配前缀的硬件信号及当前值；表达式中可用 `@signal` 引用
  - 支持行编辑、历史与 Tab 补全（命令/寄存器/信号名）

- 波形与日志
  - 调试模式下生成 `dump.fst` 波形（Verilator FST），便于配合外部波形查看器分析
  - 结束时打印追踪位置与统计信息并刷新跟踪日志

- 入口与帮助
  - 调试运行入口位于 `src/debug/debug_main.cpp`；普通仿真入口见 `src/sim/sim_main.cpp`
  - `help`：查看命令列表与用法说明
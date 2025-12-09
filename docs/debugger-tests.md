# 调试器硬件信号测试用例

## 表达式与基本查询
- `p @ifu.pc`：应输出当前 PC，与 `info r` 中 `$pc` 一致或与 `disas` 地址一致。
- `p @exu.alu_result + $a0`：应成功计算并输出结果。
- `p/x @idu.br_taken`：以十六进制显示布尔/位宽值。
- 错误名：`p @not.exist` 提示未注册信号。

## 列表与过滤
- `info s`：当信号过多时打印摘要并引导使用过滤。
- `info s exu.`：输出 EXU 前缀的信号与当前值。
- `info s wbu.rf_`：仅显示写回相关信号。

## 监视点与断点条件
- `watch @wbu.rf_wdata`：当写回数据发生变化时命中并打印变化前后值。
- `b *$pc if (@lsu.in.valid && @wbu.rf_wen)`：在同时满足条件时命中断点。

## 端到端流程（sum.bin）
- 加载 `sum.bin`，单步到写回阶段：
  - 验证 `@wbu.rf_waddr` 与 `info r` 更新的寄存器号一致。
  - 验证 `@wbu.rf_wdata` 与寄存器值一致。
- 连续运行与暂停：
  - 启用/关闭 `debug_enable` 时，信号更新仍在暂停时准确可读。

## 边界与性能
- 大量信号注册后，`info s exu.` 响应时间应可接受（<50ms）。
- 连续单步 1000 次，`@ifu.pc` 与波形文件中 PC 轨迹一致。
